/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dw.c

Abstract:

    This module implements a utility to pass idle time.

Author:

    Evan Green 23-Jul-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "swlib.h"

//
// --------------------------------------------------------------------- Macros
//

#define DwRedrawCash(_Context) \
    DwDrawStandardStat(DwString(DwsCash), 5, (_Context)->Cash, TRUE, FALSE)

#define DwRedrawWeapons(_Context)               \
    DwDrawStandardStat(DwString(DwsGuns),       \
                       7,                       \
                       (_Context)->WeaponCount, \
                       FALSE,                   \
                       FALSE)

#define DwRedrawHealth(_Context)            \
    DwDrawStandardStat(DwString(DwsHealth),            \
                       9,                   \
                       (_Context)->Health,  \
                       FALSE,               \
                       (_Context)->Health < 50)

#define DwRedrawBank(_Context) \
    DwDrawStandardStat(DwString(DwsBank), 11, (_Context)->Bank, TRUE, FALSE)

#define DwRedrawDebt(_Context) \
    DwDrawStandardStat(DwString(DwsDebt), 13, (_Context)->Debt, TRUE, TRUE)

#define DW_HEALTH_COLOR(_Context) \
    (((_Context)->Health < 50) ? ConsoleColorDarkRed : ConsoleColorBlack)

#define DwRedrawHighlightedHealth(_Context)     \
    DwDrawStat(DwString(DwsHealth),             \
               9,                               \
               (_Context)->Health,              \
               FALSE,                           \
               DW_HEALTH_COLOR(_Context),       \
               ConsoleColorGray)

#define DwDrawStandardStat(_Name, _Row, _Value, _Money, _Bad)   \
    DwDrawStat((_Name),                                         \
               (_Row),                                          \
               (_Value),                                        \
               (_Money),                                        \
               (_Bad) ? ConsoleColorDarkRed : ConsoleColorGray, \
               (_Bad) ? ConsoleColorGray : ConsoleColorDarkBlue)

#define DwClearLowerRegion() \
    SwClearRegion(ConsoleColorGray, ConsoleColorBlack, 0, 18, 80, 7)

#define DwGoodName(_Index) DwString(DwsGoodsNames + (_Index))
#define DwLocationName(_Index) DwString(DwsLocations + (_Index))
#define DwWeaponName(_Index) DwString(DwsWeapons + (_Index))

#define DwString(_Index) DwStrings[Context->Language][_Index]

//
// ---------------------------------------------------------------- Definitions
//

#define DW_VERSION_MAJOR 1
#define DW_VERSION_MINOR 0

#define DW_LANGUAGE_COUNT 2

#define DW_INITIAL_CASH 2000
#define DW_INITIAL_WEAPON_COUNT 0
#define DW_INITIAL_WEAPON_DAMAGE 0
#define DW_INITIAL_HEALTH 100
#define DW_INITIAL_BANK 0
#define DW_INITIAL_DEBT 5500
#define DW_INITIAL_SPACE 100
#define DW_INITIAL_LOCATION 0
#define DW_INITIAL_DAY 1

#define DW_GOOD_COUNT 12
#define DW_LOCATION_COUNT 6
#define DW_WEAPON_COUNT 4
#define DW_GAME_TIME 31
#define DW_SUBWAY_SAYINGS_COUNT 31
#define DW_SONG_COUNT 18
#define DW_PASSIVE_ACTIVITY_COUNT 5
#define DW_FINANCIAL_DISTRICT 0

#define DW_MORE_SPACE 10
#define DW_MIN_SPACE_PRICE 200
#define DW_MAX_SPACE_PRICE 300

#define DW_FLASH_FAST_MICROSECONDS 150000
#define DW_FLASH_SLOW_MICROSECONDS 200000

//
// Define the type of goods that brownies are made of.
//

#define DW_BROWNIE_GOOD1 2
#define DW_BROWNIE_GOOD2 11

//
// Define the factors by which extreme prices fluctuate.
//

#define DW_SURGE_FACTOR 4
#define DW_SALE_FACTOR 4

//
// Define the interest rates. Loans are brutal.
//

#define DW_LOAN_INTEREST_RATE 10
#define DW_BANK_INTEREST_RATE 5

//
// Define high score information.
//

#define DW_HIGH_SCORE_MAGIC 0x65706F44

#define DW_HIGH_SCORE_NAME_SIZE 22
#define DW_HIGH_SCORE_VALID 0x00000001
#define DW_HIGH_SCORE_ALIVE 0x00000002
#define DW_HIGH_SCORE_YOU 0x80000000
#define DW_HIGH_SCORE_COUNT 18

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for the dw application.

Members:

    Backspace - Stores the backspace character.

    Cash - Stores the amount of cash the player has.

    WeaponCount - Stores the number of weapons the player has.

    WeaponDamage - Stores the sum of the damage the player's weapons can do.

    Health - Stores the amount of health the player has.

    Bank - Stores the amount the player has in the bank.

    Debt - Stores the amount of debt the player is carrying.

    Space - Stores the empty space the player has for more goods.

    Day - Stores the current (game) day of the month.

    Location - Stores the player's location.

    Inventory - Stores the player's inventory of goods, indexed by good.

    Market - Stores today's market prices.

    ExitRequested - Stores a boolean indicating that an exit was requested.

    Language - Stores the language index in use.

--*/

typedef struct _DW_CONTEXT {
    char Backspace;
    INT Cash;
    INT WeaponCount;
    INT WeaponDamage;
    INT Health;
    INT Bank;
    INT Debt;
    INT Space;
    INT Day;
    INT Location;
    INT Inventory[DW_GOOD_COUNT];
    INT Market[DW_GOOD_COUNT];
    BOOL ExitRequested;
    INT Language;
} DW_CONTEXT, *PDW_CONTEXT;

/*++

Structure Description:

    This structure stores the data for a good.

Members:

    MinPrice - Stores the minimum price of the good.

    MaxPrice - Stores the maximum price of the good.

    Sales - Stores a boolean indicating if the good goes on sale.

    Surges - Stores a boolean indicating if the good undergoes shortages.

--*/

typedef struct _DW_GOOD {
    INT MinPrice;
    INT MaxPrice;
    BOOL Sales;
    BOOL Surges;
} DW_GOOD, *PDW_GOOD;

/*++

Structure Description:

    This structure stores the data for a location.

Members:

    PolicePresence - Stores the likelihood that the player will have to deal
        with the popo when going here.

    MinGoods - Stores the minimum number of goods at this market.

    MaxGoods - Stores the maximum number of goods at this market.

--*/

typedef struct _DW_LOCATION {
    INT PolicePresence;
    INT MinGoods;
    INT MaxGoods;
} DW_LOCATION, *PDW_LOCATION;

/*++

Structure Description:

    This structure stores the data for a weapon.

Members:

    Price - Stores the price of the weapon.

    Space - Stores the amount of space the weapon takes up.

    Damage - Stores the amount of damage the weapon does.

--*/

typedef struct _DW_WEAPON {
    INT Price;
    INT Space;
    INT Damage;
} DW_WEAPON, *PDW_WEAPON;

/*++

Structure Description:

    This structure stores a single high score entry.

Members:

    Flags - Stores a bitfield of flags about this entry.

    Year - Stores the year of the high score.

    Month - Stores the month of the high score, 1-12.

    Day - Stores the day of the high score, 1-31.

    Amount - Stores the amount they had.

    Name - Stores the high scorer's name.

--*/

typedef struct _DW_HIGH_SCORE_ENTRY {
    ULONG Flags;
    USHORT Year;
    UCHAR Month;
    UCHAR Day;
    LONG Amount;
    CHAR Name[DW_HIGH_SCORE_NAME_SIZE];
} DW_HIGH_SCORE_ENTRY, *PDW_HIGH_SCORE_ENTRY;

/*++

Structure Description:

    This structure stores the complete high score data.

Members:

    Magic - Stores the magic value DW_HIGH_SCORE_MAGIC.

    Checksum - Stores the checksum of the table, assuming the checksum field
        itself is zero.

    Entries - Stores the array of high score entries.

--*/

typedef struct _DW_HIGH_SCORES {
    ULONG Magic;
    ULONG Checksum;
    DW_HIGH_SCORE_ENTRY Entries[DW_HIGH_SCORE_COUNT];
} DW_HIGH_SCORES, *PDW_HIGH_SCORES;

typedef enum _DW_STRING_VALUE {
    DwsIntroTitle,
    DwsIntroText,
    DwsHorizontalLine,
    DwsTwoColumnLine,
    DwsColumnTitles,
    DwsSubway,
    DwsMarketGreeting,
    DwsSurgeFormat1,
    DwsSurgeFormat2,
    DwsPressSpace,
    DwsBuyOrJet,
    DwsBuySellJet,
    DwsWhatToBuy,
    DwsWhatToSell,
    DwsHowManyToBuy,
    DwsHowManyToSell,
    DwsWhereTo,
    DwsSubwayLadyFormat,
    DwsSubwayQualifier,
    DwsHearSongFormat,
    DwsProductOfferFormat,
    DwsProductMoreSpace,
    DwsMugged,
    DwsReceiveGiftFormat,
    DwsSendGiftFormat,
    DwsLostGoodsFormat,
    DwsFoundGoodsFormat,
    DwsSharedGoodsFormat,
    DwsSirenSong,
    DwsSirenPrompt,
    DwsSirenResult,
    DwsPassiveActivityFormat,
    DwsFightThreatFormat,
    DwsRunOrFight,
    DwsRunOption,
    DwsFight,
    DwsRun,
    DwsPlayerFire,
    DwsPlayerMissed,
    DwsPlayerHit,
    DwsPlayerUnderFire,
    DwsFled,
    DwsFailedToFlee,
    DwsNotFleeing,
    DwsTheyMissed,
    DwsTheyHit,
    DwsKilled,
    DwsFightVictoryFormat,
    DwsDoctorOffer,
    DwsVisitLoanShark,
    DwsYes,
    DwsLoanRepaymentAmount,
    DwsVisitBank,
    DwsDepositOrWithdraw,
    DwsHowMuchMoney,
    DwsHighScoresTitle,
    DwsHighScoreDead,
    DwsHighScoreFormat,
    DwsPlayAgain,
    DwsMadeHighScores,
    DwsNamePrompt,
    DwsAnonymous,
    DwsYou,
    DwsGoodsNames,
    DwsGoodsSales = DwsGoodsNames + DW_GOOD_COUNT,
    DwsLocations = DwsGoodsSales + DW_GOOD_COUNT,
    DwsWeapons = DwsLocations + DW_LOCATION_COUNT,
    DwsSubwaySayings = DwsWeapons + DW_WEAPON_COUNT,
    DwsSongs = DwsSubwaySayings + DW_SUBWAY_SAYINGS_COUNT,
    DwsPassiveActivities = DwsSongs + DW_SONG_COUNT,
    DwsCash = DwsPassiveActivities + DW_PASSIVE_ACTIVITY_COUNT,
    DwsGuns,
    DwsHealth,
    DwsBank,
    DwsDebt,
    DwsAccess,
    DwsNoAccess,
    DwsStringCount
} DW_STRING_VALUE, *PDW_STRING_VALUE;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DwDecodeStrings (
    PDW_CONTEXT Context
    );

VOID
DwDrawIntro (
    PDW_CONTEXT Context
    );

VOID
DwDrawGameBoard (
    PDW_CONTEXT Context
    );

VOID
DwResetGame (
    PDW_CONTEXT Context
    );

VOID
DwPlay (
    PDW_CONTEXT Context
    );

VOID
DwDoDailyEvents (
    PDW_CONTEXT Context
    );

VOID
DwReceiveOffer (
    PDW_CONTEXT Context
    );

VOID
DwPerformActOfGod (
    PDW_CONTEXT Context
    );

VOID
DwEncounterPolice (
    PDW_CONTEXT Context
    );

VOID
DwVisitFinancialDistrict (
    PDW_CONTEXT Context
    );

VOID
DwGenerateMarket (
    PDW_CONTEXT Context
    );

VOID
DwParticipateInMarket (
    PDW_CONTEXT Context
    );

VOID
DwDrawMarket (
    PDW_CONTEXT Context
    );

VOID
DwRedrawInventory (
    PDW_CONTEXT Context
    );

VOID
DwPresentNotification (
    PDW_CONTEXT Context,
    PSTR Notification
    );

VOID
DwDrawBottomPrompt (
    PSTR Prompt
    );

VOID
DwDrawStat (
    PSTR Name,
    INT Row,
    INT Value,
    BOOL Money,
    CONSOLE_COLOR Foreground,
    CONSOLE_COLOR Background
    );

VOID
DwDrawLocation (
    PSTR Location
    );

VOID
DwRedrawSpace (
    PDW_CONTEXT Context
    );

VOID
DwFlashText (
    PDW_CONTEXT Context,
    PSTR String,
    INT XPosition,
    INT YPosition,
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    CONSOLE_COLOR FlashForeground,
    BOOL Fast
    );

INT
DwDisplayHighScores (
    PDW_CONTEXT Context
    );

VOID
DwLoadHighScores (
    PDW_CONTEXT Context,
    PDW_HIGH_SCORES Scores
    );

INT
DwHighScoresFileIo (
    PDW_HIGH_SCORES Scores,
    BOOL Load
    );

int
DwCompareHighScores (
    const void *LeftPointer,
    const void *RightPointer
    );

VOID
DwFormatMoney (
    PSTR String,
    UINTN StringSize,
    INT Value
    );

INT
DwReadCharacterSet (
    PDW_CONTEXT Context,
    PSTR Set
    );

INT
DwReadQuantity (
    PDW_CONTEXT Context
    );

INT
DwReadString (
    PDW_CONTEXT Context,
    PSTR String,
    INT StringSize
    );

INT
DwReadYesNoAnswer (
    PDW_CONTEXT Context,
    PSTR Exposition,
    PSTR Prompt
    );

INT
DwReadCharacter (
    PDW_CONTEXT Context
    );

INT
DwRandom (
    INT Minimum,
    INT Maximum
    );

ULONG
DwChecksum (
    PVOID Buffer,
    ULONG Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the menu of available goods and how they behave. Don't look!
//

DW_GOOD DwGoods[DW_GOOD_COUNT] = {
    {1000, 4000, TRUE, FALSE},
    {15000, 29000, FALSE, TRUE},
    {480, 1280, TRUE, FALSE},
    {5500, 13000, FALSE, TRUE},
    {11, 60, TRUE, FALSE},
    {1500, 4400, FALSE, FALSE},
    {540, 1250, FALSE, TRUE},
    {1000, 2500, FALSE, FALSE},
    {220, 700, FALSE, FALSE},
    {630, 1300, FALSE, FALSE},
    {90, 250, FALSE, TRUE},
    {315, 890, TRUE, FALSE}
};

DW_LOCATION DwLocations[DW_LOCATION_COUNT] = {
    {10, (DW_GOOD_COUNT / 2) + 2, DW_GOOD_COUNT},
    {5, (DW_GOOD_COUNT / 2) + 3, DW_GOOD_COUNT},
    {15, (DW_GOOD_COUNT / 2) + 1, DW_GOOD_COUNT},
    {90, (DW_GOOD_COUNT / 2), DW_GOOD_COUNT - 2},
    {20, (DW_GOOD_COUNT / 2) + 1, DW_GOOD_COUNT},
    {70, (DW_GOOD_COUNT / 2), DW_GOOD_COUNT - 1},
};

DW_WEAPON DwWeapons[DW_WEAPON_COUNT] = {
    {300, 4, 5},
    {350, 4, 9},
    {290, 4, 4},
    {310, 4, 7},
};

//
// Define all the strings used in the game, localized. Do a very minimal effort
// to hide from prying "strings".
//

PSTR DwStrings[DW_LANGUAGE_COUNT][DwsStringCount] = {
    {
        "E!P!Q!F!!!X!B!S!T",
        "!Cbtfe!po!Kpio!F/!Efmm(t!pme!Esvh!Xbst!hbnf-!Epqf!Xbst!jt!b!"         \
            "tjnvmbujpo!pg!bo\n!jnbhjobsz!esvh!nbslfu/!!Epqf!xbst!jt!bo!"      \
            "Bmm.Bnfsjdbo!hbnf!xijdi!gfbuvsft\n!cvzjoh-!tfmmjoh-!boe!uszjoh!"  \
            "up!hfu!qbtu!uif!dpqt\"\n\n!Uif!gjstu!uijoh!zpv!offe!up!ep!jt!"    \
            "qbz!pgg!zpvs!efcu!up!uif!Mpbo!Tibsl/!!Bgufs\n!uibu-!zpvs!hpbm!"   \
            "jt!up!nblf!bt!nvdi!npofz!bt!qpttjcmf!)boe!tubz!bmjwf*\"!!Zpv\n!"  \
            "ibwf!pof!npoui!pg!hbnf!ujnf!up!nblf!zpvs!gpsuvof/\n\n!Epqf!Xbst!" \
            "ibt!cffo!cspvhiu!up!zpv!dpvsuftz!pg!uif!Ibqqz!Ibdlfs!Gpvoebujpo/",

        ",>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" \
            ">>>>>>>>>,",

        "}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" \
            "!!!!!!!!!}",

        "}!!!!!!!!!!!!Tubut!!!!!!!!!!!!}!Dfousbm!Qbsl!!}!!!!!!!!!!Usfodidpbu!" \
            "!!!!!!!!!}",

        "!T!V!C!X!B!Z",
        "Ifz!evef-!uif!qsjdft!pg!esvht!ifsf!bsf;",
        "Dpqt!nbef!b!cjh!&t!cvtu\"!!Qsjdft!bsf!pvusbhfpvt\"",
        "Beejdut!bsf!cvzjoh!&t!bu!pvusbhfpvt!qsjdft\"",
        "Qsftt!TQBDF!up!dpoujovf",
        "Xjmm!zpv!C?vz-!ps!K?fu@!",
        "Xjmm!zpv!C?vz-!T?fmm-!ps!K?fu@!",
        "Xibu!ep!zpv!xjti!up!cvz@!",
        "Xibu!ep!zpv!xjui!up!tfmm@!",
        "Zpv!dbo!bggpse!&e/!!Ipx!nboz!ep!zpv!cvz@!",
        "Zpv!ibwf!&e/!!Ipx!nboz!ep!zpv!tfmm@!",
        "Xifsf!up-!evef@",
        "Uif!mbez!ofyu!up!zpv!po!uif!tvcxbz!tbje-\n!!!!!#&t#///\n!&t",
        ")bu!mfbtu-!zpv!.uijol.!uibu(t!xibu!tif!tbje*/",
        "Zpv!ifbs!tpnfpof!qmbzjoh!&t",
        "Xpvme!zpv!mjlf!up!cvz!b!&t!gps!%&e@!",
        "cjhhfs!usfodidpbu",
        "Zpv!xfsf!nvhhfe!jo!uif!tvcxbz\"",
        "Zpv!nffu!b!gsjfoe\"!!If!mbzt!tpnf!&t!po!zpv/",
        "Zpv!nffu!b!gsjfoe\"!!Zpv!hjwf!ifs!tpnf!&t/",
        "Qpmjdf!epht!dibtfe!zpv!gps!&e!cmpdlt\"\n!Zpv!espqqfe!tpnf!esvht\"!!" \
            "Uibu(t!b!esbh-!nbo/",

        "Zpv!gjoe!&e!vojut!pg!&t!po!b!efbe!evef!jo!uif!tvcxbz/",
        "Zpvs!nbnb!nbef!cspxojft!xjui!tpnf!pg!zpvs!&t\"!!Uifz!xfsf!hsfbu\"",
        "Uifsf!jt!tpnf!xffe!uibu!tnfmmt!mjlf!qbsbrvbu!ifsf\"!!Ju!mpplt!hppe\"",
        "Xjmm!zpv!tnplf!ju@!",
        "Zpv!ibmmvdjobufe!gps!uisff!ebzt!po!uif!xjmeftu!usjq!zpv!fwfs!" \
            "jnbhjofe\"\n!Uifo!zpv!ejfe!cfdbvtf!zpvs!csbjo!ejtjoufhsbufe\"",

        "Zpv!tupqqfe!up!&t/",
        "Pggjdfs!Ibsebtt!boe!&e!pg!ijt!efqvujft!bsf!dibtjoh!zpv\"",
        "Xjmm!zpv!S?vo!ps!G?jhiu@!",
        "Xjmm!zpv!svo@!",
        "Gjhiu",
        "Svo",
        "Zpv(sf!gjsjoh!po!uifn\"!!",
        "Zpv!njttfe\"",
        "Zpv!ljmmfe!pof\"",
        "Uifz!bsf!gjsjoh!po!zpv-!nbo\"!!",
        "Zpv!mptu!uifn!jo!uif!bmmfzt/",
        "Zpv!dbo(u!mptf!uifn\"",
        "Zpv!tuboe!uifsf!mjlf!bo!jejpu/",
        "Uifz!njttfe\"",
        "Zpv(wf!cffo!iju\"",
        "Uifz!xbtufe!zpv!nbo\"!!Xibu!b!esbh\"",
        "Zpv!ljmmfe!bmm!pg!uifn\"\n!Zpv!gjoe!%&e!po!Pggjdfs!Ibsebtt(!" \
            "dbsdbtt\"\n!",

        "Xjmm!zpv!qbz!%&e!up!ibwf!b!epdups!tfx!zpv!vq@!",
        "Xpvme!zpv!mjlf!up!wjtju!uif!Mpbo!Tibsl@!",
        "Zft",
        "Ipx!nvdi!ep!zpv!hjwf!ijn@!",
        "Xpvme!zpv!mjlf!up!wjtju!uif!Cbol@!",
        "Ep!zpv!xbou!up!E?fqptju!ps!X?juiesbx@!",
        "Ipx!nvdi!npofz@!",
        "I!J!H!I!!!T!D!P!S!F!T",
        ")S/J/Q/*",
        "&23t!!!!!!!!!!&13e.&13e.&15e!!!!!!!!!!&.31t&t",
        "Qmbz!bhbjo@",
        "Dpohsbuvmbujpot\"!!Zpv!nbef!uif!Ijhi!Tdpsft!mjtu\"\n!",
        "Qmfbtf!foufs!zpvs!obnf;!",
        "Nztufsz!Efbmfs",
        "+++!ZPV!+++",
        "Bdje",
        "Dpdbjof",
        "Ibtijti",
        "Ifspjo",
        "Mveft",
        "NEB",
        "Pqjvn",
        "QDQ",
        "Qfzpuf",
        "Tisppnt",
        "Tqffe",
        "Xffe",
        "Uif!nbslfu!ibt!cffo!gmppefe!xjui!difbq!ipnf.nbef!bdje\"",
        NULL,
        "Uif!Nbssblfti!Fyqsftt!ibt!bssjwfe\"",
        NULL,
        "Sjwbm!esvh!efbmfst!sbjefe!b!qibsnbdz!boe!bsf!tfmmjoh!difbq!mveft\"",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "Dpmvncjbo!gsfjhiufs!evtufe!uif!Dpbtu!Hvbse\"\n!Xffe!qsjdft!ibwf!" \
            "cpuupnfe!pvu\"",

        "Cspoy",
        "Hifuup",
        "Dfousbm!Qbsl",
        "Nboibuubo",
        "Dpofz!Jtmboe",
        "Cspplmzo",
        "Cbsfuub",
        "/49!Tqfdjbm",
        "Svhfs",
        "Tbuvsebz!Ojhiu!Tqfdjbm",
        "Xpvmeo(u!ju!cf!gvooz!jg!fwfszpof!tveefomz!rvbdlfe!bu!podf@",
        "Uif!Qpqf!xbt!podf!Kfxjti-!zpv!lopx",
        "J(mm!cf!zpv!ibwf!tpnf!sfbmmz!joufsftujoh!esfbnt",
        "Tp!J!uijol!J(n!hpjoh!up!Bntufsebn!uijt!zfbs",
        "Tpo-!zpv!offe!b!zfmmpx!ibjsdvu",
        "J!uijol!ju(t!xpoefsgvm!xibu!uifz(sf!epjoh!xjui!jodfotf!uiftf!ebzt",
        "J!xbto(u!bmxbzt!b!xpnbo-!zpv!lopx",
        "Epft!zpvs!npuifs!lopx!zpv(sf!b!epqf!efbmfs@",
        "Bsf!zpv!ijhi!po!tpnfuijoh@",
        "Pi-!zpv!nvtu!cf!gspn!Dbmjgpsojb",
        "J!vtfe!up!cf!b!ijqqjf-!nztfmg",
        "Uifsf(t!opuijoh!mjlf!ibwjoh!mput!pg!npofz",
        "Zpv!mppl!mjlf!bo!bbsewbsl\"",
        "J!epo(u!cfmjfwf!jo!Spobme!Sfbhbo",
        "Dpvsbhf\"!!Cvti!jt!b!oppemf\"",
        "Ibwfo(u!J!tffo!zpv!po!UW@",
        "J!uijol!ifnpssipje!dpnnfsdjbmt!bsf!sfbmmz!ofbu\"",
        "Xf(sf!xjoojoh!uif!xbs!gps!esvht\"",
        "B!ebz!xjuipvu!epqf!jt!mjlf!ojhiu",
        "Xf!pomz!vtf!31&!pg!pvs!csbjot-!tp!xiz!opu!cvso!pvu!uif!puifs!91&",
        "J(n!tpmjdjujoh!dpousjcvujpot!gps![pncjft!gps!Disjtu",
        "J(e!mjlf!up!tfmm!zpv!bo!fejcmf!qppemf",
        "Xjoofst!epo(u!ep!esvht///!vomftt!uifz!ep",
        "Ljmm!b!dpq!gps!Disjtu\"",
        "J!bn!uif!xbmsvt\"",
        "Kftvt!mpwft!zpv!npsf!uibo!zpv!xjmm!lopx",
        "J!gffm!bo!vobddpvoubcmf!vshf!up!ezf!nz!ibjs!cmvf",
        "Xbto(u!Kbof!Gpoeb!xpoefsgvm!jo!Cbscbsfmmb",
        "Kvtu!tbz!Op//!xfmm-!nbzcf///!pl-!xibu!uif!ifmm\"",
        "Xpvme!zpv!mjlf!b!kfmmz!cbcz@",
        "Esvht!dbo!cf!zpvs!gsjfoe\"",
        "#Bsf!Zpv!Fyqfsjfodfe#!cz!Kjnj!Ifoesjy",
        "#Diffcb!Diffcb#!cz!Upof!Mpd",
        "#Dpnjo(!jo!up!Mpt!Bohfmft#!cz!Bsmp!Hvuisjf",
        "#Dpnnfsdjbm#!cz!Tqbolz!boe!Pvs!Hboh",
        "#Mbuf!jo!uif!Fwfojoh#!cz!Qbvm!Tjnpo",
        "#Mjhiu!Vq#!cz!Tuzy",
        "#Nfyjdp#!cz!Kfggfstpo!Bjsqmbof",
        "#Pof!Uplf!Pwfs!uif!Mjof#!cz!Csfxfs!'!Tijqmfz",
        "#Uif!Tnplfpvu#!cz!Tifm!Tjmwfstufjo",
        "#Xijuf!Sbccju#!cz!Kfggfstpo!Bjsqmbof",
        "#Judizdpp!Qbsl#!cz!Tnbmm!Gbdft",
        "#Xijuf!Qvolt!po!Epqf#!cz!Uif!Uvcft",
        "#Mfhfoe!pg!b!Njoe#!cz!Uif!Nppez!Cmvft",
        "#Fjhiu!Njmft!Ijhi#!cz!Uif!Czset",
        "#Bdbqvmdp!Hpme#!cz!Sjefst!pg!uif!Qvsqmf!Tbhf",
        "#Ljdlt#!cz!Qbvm!Sfwfsf!'!uif!Sbjefst",
        "uif!Ojypo!ubqft",
        "#Mfhbmj{f!Ju#!cz!Npkp!Ojypo!'!Tlje!Spqfs",
        "ibwf!b!cffs",
        "tnplf!b!kpjou",
        "tnplf!b!djhbs",
        "tnplf!b!Ekbsvn",
        "tnplf!b!djhbsfuuf",
        "Dbti",
        "Hvot",
        "Ifbmui",
        "Cbol",
        "Efcu",
        "EX`QMBZ`PME`TDIPPM",
        "ti;!ex;!Dpnnboe!opu!gpvoe/\n",
    },
    {
        "D!B!O!E!Z!!!X!B!S!T",
        "!Cbtfe!po!Kpio!F/!Efmm(t!pme!usbejoh!hbnf-!Dboez!Xbst!jt!b!"          \
            "tjnvmbujpo!pg!bo\n!jnbhjobsz!dboez!nbslfu/!!Dboez!Xbst!jt!bo!"    \
            "Bmm.Bnfsjdbo!hbnf!xijdi!gfbuvsft\n!cvzjoh-!tfmmjoh-!boe!uszjoh!"  \
            "up!hfu!qbtu!uif!wfhhjf!qpmjdf\"\n\n!Uif!gjstu!uijoh!zpv!offe!"    \
            "up!ep!jt!qbz!pgg!zpvs!efcu!up!uif!Mpbo!Tibsl/!!Bgufs\n!uibu-!"    \
            "zpv!hpbm!jt!up!nblf!bt!nvdi!npofz!bt!qpttjcmf!)boe!tubz!"         \
            "bmjwf*\"!!Zpv\n!ibwf!pof!npoui!pg!hbnf!ujnf!up!nblf!zpvs!"        \
            "gpsuvof/\n\n",

        ",>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" \
            ">>>>>>>>>>>>,",

        "}!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!}!!!!!!!!!!!!!!!!!!!!!!!!!!" \
            "!!!!!!!!!!!!}",

        "}!!!!!!!!!!!!Tubut!!!!!!!!!!!!}!Dfousbm!Qbsl!!}!!!!!!!!!!Usfodidpbu" \
            "!!!!!!!!!!}",

        "!!C!!!V!!!T!",
        "Ifz!evef-!uif!qsjdft!pg!dboez!ifsf!bsf;",
        "Uif!GEB!nbef!b!cjh!&t!cvtu\"!!Qsjdft!bsf!pvusbhfpvt\"",
        "Beejdut!bsf!cvzjoh!&t!bu!pvusbhfpvt!qsjdft\"",
        "Qsftt!TQBDF!up!dpoujovf",
        "Xjmm!zpv!C?vz-!ps!K?fu@!",
        "Xjmm!zpv!C?vz-!T?fmm-!ps!K?fu@!",
        "Xibu!ep!zpv!xjti!up!cvz@!",
        "Xibu!ep!zpv!xjui!up!tfmm@!",
        "Zpv!dbo!bggpse!&e/!!Ipx!nboz!ep!zpv!cvz@!",
        "Zpv!ibwf!&e/!!Ipx!nboz!ep!zpv!tfmm@!",
        "Xifsf!up-!evef@",
        "Uif!mbez!ofyu!up!zpv!po!uif!cvt!tbje-\n!!!!!#&t#///\n!&t",
        ")bu!mfbtu-!zpv!.uijol.!uibu(t!xibu!tif!tbje*/",
        "Zpv!ifbs!tpnfpof!qmbzjoh!&t",
        "Xpvme!zpv!mjlf!up!cvz!b!&t!gps!%&e@!",
        "cjhhfs!usfodidpbu",
        "Zpv!xfsf!nvhhfe!jo!uif!cvt!tubujpo\"",
        "Zpv!nffu!b!gsjfoe\"!!If!mbzt!tpnf!&t!po!zpv/",
        "Zpv!nffu!b!gsjfoe\"!!Zpv!hjwf!ifs!tpnf!&t/",
        "Dsb{fe!dijmesfo!dibtfe!zpv!gps!&e!cmpdlt\"\n!Zpv!espqqfe!tpnf!" \
            "dboez\"!!Uibu(t!b!esbh-!nbo/",

        "Zpv!gjoe!&e!vojut!pg!&t!bu!uif!cpuupn!pg!b!wfoejoh!nbdijof/",
        "Zpvs!nbnb!nbef!cspxojft!xjui!tpnf!pg!zpvs!&t\"!!Uifz!xfsf!hsfbu\"",
        "Uifsf!jt!tpnf!tpeb!ifsf!uibu!mpplt!mjlf!Ofx!Dplf\"!!Ju!mpplt!mfhju\"",
        "Xjmm!zpv!esjol!ju@!",
        "Zpv!ibmmvdjobufe!gps!uisff!ebzt!po!uif!xjmeftu!usjq!zpv!fwfs!" \
            "jnbhjofe\"\n!Uifo!zpv!ejfe!cfdbvtf!zpvs!csbjo!ejtjoufhsbufe\"",

        "Zpv!tupqqfe!up!&t/",
        "Dbqubjo!Wfhfubcmft!boe!&e!pg!ijt!dspojft!bsf!dibtjoh!zpv\"",
        "Xjmm!zpv!S?vo!ps!G?jhiu@!",
        "Xjmm!zpv!svo@!",
        "Gjhiu",
        "Svo",
        "Zpv(sf!gjsjoh!po!uifn\"!!",
        "Zpv!njttfe\"",
        "Zpv!hpu!pof\"",
        "Uifz!bsf!gjsjoh!po!zpv-!nbo\"!!",
        "Zpv!mptu!uifn!jo!uif!tvqfsnbslfu/",
        "Zpv!dbo(u!mptf!uifn\"",
        "Zpv!tuboe!uifsf!mjlf!bo!jejpu/",
        "Uifz!njttfe\"",
        "Zpv(wf!cffo!iju\"",
        "Uifz!dpowfsufe!zpv!up!b!wfhbo\"!!Xibu!b!esbh\"",
        "Zpv!hpu!bmm!pg!uifn\"\n!Zpv!gjoe!%&e!po!Dbqubjo!Wfhfubcmft(!" \
            "usvol\"\n!",

        "Xjmm!zpv!qbz!%&e!up!ibwf!b!epdups!gjy!zpv!vq@!",
        "Xpvme!zpv!mjlf!up!wjtju!uif!Mpbo!Tibsl@!",
        "Zft",
        "Ipx!nvdi!ep!zpv!hjwf!ijn@!",
        "Xpvme!zpv!mjlf!up!wjtju!uif!Cbol@!",
        "Ep!zpv!xbou!up!E?fqptju!ps!X?juiesbx@!",
        "Ipx!nvdi!npofz@!",
        "I!J!H!I!!!T!D!P!S!F!T",
        ")wfhbo*",
        "&23t!!!!!!!!!!&13e.&13e.&15e!!!!!!!!!!&.31t&t",
        "Qmbz!bhbjo@",
        "Dpohsbuvmbujpot\"!!Zpv!nbef!uif!Ijhi!Tdpsft!mjtu\"\n!",
        "Qmfbtf!foufs!zpvs!obnf;!",
        "Nztufsz!Efbmfs",
        "+++!ZPV!+++",
        "BjsIfbet",
        "Dbecvsz",
        "Ljttft",
        "Tljuumft",
        "Ujd!Ubdt",
        "Nbmu!Cbmmt",
        "Qjyjf!Tujy",
        "Qpq!Spdlt",
        "Qf{",
        "Cmpx!Qpqt",
        "Tubscvstu",
        "N'Nt",
        "Uif!nbslfu!ibt!cffo!gmppefe!xjui!difbq!ipnf.nbef!BjsIfbet\"",
        NULL,
        "Ju(t!Wbmfoujof(t!Ebz\"!Ljttft!bsf!po!tbmf\"",
        NULL,
        "Sjwbm!hspdfsz!tupsft!bsf!tfmmjoh!difbq!pgg.csboe!Ujd.Ubdt\"",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "B!Ifstifz(t!usvdl!dsbtife!po!uif!ijhixbz\"\nN'Nt!bsf!fwfszxifsf-!" \
            "qsjdft!ibwf!cpuupnfe!pvu\"",

        "Njttjpo",
        "Dbtusp",
        "Qsftjejp",
        "Tvotfu",
        "Svttjbo!Ijmm",
        "Qpusfsp",
        "Tvqfs!Tpblfs",
        "Qfmmfu!Hvo",
        "Sfe!Szefs",
        "Dbq!Hvo",
        "Xpvmeo(u!ju!cf!gvooz!jg!fwfszpof!tveefomz!rvbdlfe!bu!podf@",
        "Uif!Qpqf!xbt!podf!Kfxjti-!zpv!lopx",
        "J(mm!cf!zpv!ibwf!tpnf!sfbmmz!joufsftujoh!esfbnt",
        "Tp!J!uijol!J(n!hpjoh!up!Qfootzmwbojb!uijt!zfbs",
        "Tpo-!zpv!offe!b!zfmmpx!ibjsdvu",
        "J!uijol!ju(t!xpoefsgvm!xibu!uifz(sf!epjoh!xjui!nbqmf!tzsvq!uiftf!ebzt",
        "J!xbto(u!bmxbzt!b!xpnbo-!zpv!lopx",
        "Epft!zpvs!npuifs!lopx!zpv!fbu!tp!nvdi!dboez@",
        "Zpv(sf!tvsf!cpvodz-!bsfo(u!zpv@",
        "Pi-!zpv!nvtu!cf!gspn!Dbmjgpsojb",
        "J!vtfe!up!cf!b!ijqqjf-!nztfmg",
        "Uifsf(t!opuijoh!mjlf!ibwjoh!mput!pg!npofz",
        "Zpv!mppl!mjlf!bo!bbsewbsl\"",
        "J!epo(u!cfmjfwf!jo!dmbttjdbm!qiztjdt",
        "Dpvsbhf\"!!Tjsj!jt!b!oppemf\"",
        "Ibwfo(u!J!tffo!zpv!po!UW@",
        "J!uijol!Hfpshf!Gpsfnbo!hsjmmt!bsf!sfbmmz!ofbu\"",
        "Xf(sf!xjoojoh!uif!xbs!gps!dpuupo!dboez\"",
        "B!ebz!xjuipvu!dboez!jt!mjlf!ojhiu",
        "J!dbo(u!cfmjfwf!J(n!opu!cvuufs\"",
        "J(n!tpmjdjujoh!dpousjcvujpot!gps![pncjft!gps!Cveeib",
        "J(e!mjlf!up!tfmm!zpv!fejcmf!upjmfu!qbqfs",
        "Ofwfs!Yfspy!cvccmf!hvn",
        "Nz!Lju.Lbu!cbst!dpnf!jo!gjwft\"",
        "J!bn!uif!xbmsvt\"",
        "Kftvt!mpwft!zpv!npsf!uibo!zpv!xjmm!lopx",
        "J!gffm!bo!vobddpvoubcmf!vshf!up!ezf!nz!ibjs!cmvf",
        "Xbto(u!Kbof!Gpoeb!xpoefsgvm!jo!Cbscbsfmmb",
        "Opuijoh!siznft!xjui!psbohf!///!fydfqu!cpsbohf\"",
        "Xpvme!zpv!mjlf!b!kfmmz!cbcz@",
        "Dboez!dbo!cf!zpvs!gsjfoe\"",
        "#J!Xbou!Dboez#!cz!Cpx!Xpx!Xpx",
        "#Npuifs!Qpqdpso#!cz!Kbnft!Cspxo",
        "#J!Dbo(u!Ifmq!Nztfmg#!cz!Uif!Gpvs!Upqt",
        "#Tvhbs-!Tvhbs#!cz!Uif!Bsdijft",
        "#Dvqt!boe!Dblft#!cz!TqjobmUbq",
        "#D!jt!gps!Dppljf#!cz!uif!Dppljf!Npotufs",
        "#Mft!Tvdfuuft#!cz!Gsbodf!Hbmmf",
        "#Dboeznbo#!cz!Brvb-!Brvb",
        "#Tbwpz!Usvggmf#!cz!Uif!Cfbumft",
        "#Dipdpmbuf!Kftvt#!cz!Upn!Xbjut",
        "#Tusbxcfssz!Gjfmet!Gpsfwfs#!cz!Uif!Cfbumft",
        "#Tvhbs!Nbhopmjb#!cz!Uif!Hsbufgvm!Efbe",
        "#Cspxo!Tvhbs#!cz!Uif!Spmmjoh!Tupoft",
        "#Qfbdift#!cz!Uif!Qsftjefout!pg!uif!Vojufe!Tubuft",
        "#Xjme!Ipofz!Qjf#!cz!Uif!Cfbumft",
        "#Ebodf!pg!uif!Tvhbs!Qmvn!Gbjsz#!cz!Udibjlpwtlz",
        "#B!Tqppogvm!pg!Tvhbs#!cz!Kvmjf!Boesfxt",
        "#Qpvs!Tpnf!Tvhbs!Po!Nf#!cz!Efg!Mfqqbse",
        "ibwf!b!cvshfs",
        "ibwf!tpnf!dpggff",
        "hsbc!b!rvjdl!gjy!zpvstfmg",
        "hsbc!tpnf!mvodi",
        "ibwf!b!dboez!djhbsfuuf",
        "Npofz",
        "Xfbqpot",
        "Ifbmui",
        "Cbol",
        "Efcu",
        "EX`QMBZ",
        "ti;!ex;!Dpnnboe!opu!gpvoe/\n",
    },
};

BOOL DwStringsDecoded;
INT DwRandomSource;

//
// ------------------------------------------------------------------ Functions
//

INT
DwMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the dw utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    DW_CONTEXT Context;
    INT PlayAgain;
    INT Result;

    //
    // Seed the bad randomizer, and try to open a decent random source.
    //

    DwRandomSource = SwOpen("/dev/urandom", O_RDONLY | O_NONBLOCK, 0);
    if (DwRandomSource < 0) {
        DwRandomSource = SwOpen("/dev/random", O_RDONLY | O_NONBLOCK, 0);
    }

    srand(time(NULL));
    memset(&Context, 0, sizeof(Context));
    Result = DwDecodeStrings(&Context);
    if (Result != 0) {
        goto MainEnd;
    }

    SwSetRawInputMode(NULL, NULL);
    DwDrawIntro(&Context);
    if (DwReadCharacterSet(&Context, " ") == -1) {
        goto MainEnd;
    }

    //
    // Shall we play a game?
    //

    do {
        DwResetGame(&Context);
        DwPlay(&Context);
        PlayAgain = DwDisplayHighScores(&Context);

    } while (PlayAgain == 1);

    SwClearRegion(ConsoleColorDefault, ConsoleColorDefault, 0, 0, 80, 25);
    SwMoveCursor(stdout, 0, 0);
    Result = 0;

MainEnd:
    if (DwRandomSource >= 0) {
        close(DwRandomSource);
    }

    SwRestoreInputMode();
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwDecodeStrings (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine decodes all the strings used the app.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

    127 if the proper conditions were not met.

--*/

{

    PSTR Buffer;
    ULONG Language;
    PSTR NewString;
    BOOL Print;
    size_t Size;
    PSTR Source;
    DW_STRING_VALUE StringIndex;

    if (DwStringsDecoded != FALSE) {
        return 0;
    }

    Print = FALSE;
    Size = 0;
    for (Language = 0; Language < DW_LANGUAGE_COUNT; Language += 1) {
        for (StringIndex = 0; StringIndex < DwsStringCount; StringIndex += 1) {
            Source = DwStrings[Language][StringIndex];
            if (Source != NULL) {
                Size += strlen(Source) + 1;
            }
        }
    }

    Buffer = malloc(Size);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    for (Language = 0; Language < DW_LANGUAGE_COUNT; Language += 1) {
        if (Print != FALSE) {
            printf("    {\n");
        }

        for (StringIndex = 0; StringIndex < DwsStringCount; StringIndex += 1) {
            Source = DwStrings[Language][StringIndex];
            if (Source != NULL) {
                NewString = Buffer;
                if (Print != FALSE) {
                    printf("        \"");
                }

                while (*Source != '\0') {
                    if (*Source == '\n') {
                        *Buffer = '\n';

                    } else {
                        *Buffer = *Source - 1;
                    }

                    if (Print != FALSE) {
                        if (*Buffer == '\n') {
                            printf("\\n");

                        } else if (*Buffer == '"') {
                            printf("\\\"");

                        } else {
                            printf("%c", *Buffer);
                        }
                    }

                    Source += 1;
                    Buffer += 1;
                }

                if (Print != FALSE) {
                    printf("\",\n");
                }

                *Buffer = '\0';
                Buffer += 1;
                DwStrings[Language][StringIndex] = NewString;

            } else {
                if (Print != FALSE) {
                    printf("        NULL,\n");
                }
            }
        }

        if (Print != FALSE) {
            printf("    },\n");
        }
    }

    DwStringsDecoded = TRUE;
    for (Language = 0; Language < DW_LANGUAGE_COUNT; Language += 1) {
        if (getenv(DwStrings[Language][DwsAccess]) != NULL) {
            Context->Language = Language;
            break;
        }
    }

    if (Language == DW_LANGUAGE_COUNT) {
        fprintf(stderr, "%s", DwString(DwsNoAccess));
        return 127;
    }

    return 0;
}

VOID
DwDrawIntro (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine prepares the console and draws the introductory screen.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    SwClearRegion(ConsoleColorGray, ConsoleColorDefault, 0, 0, 80, 25);
    SwMoveCursor(stdout, 30, 0);
    SwPrintInColor(ConsoleColorGray,
                   ConsoleColorDarkBlue,
                   DwString(DwsIntroTitle));

    SwMoveCursor(stdout, 0, 5);
    SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, DwString(DwsIntroText));
    DwDrawBottomPrompt(DwString(DwsPressSpace));
    return;
}

VOID
DwResetGame (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine resets the game context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Index;

    Context->Backspace = 0x7F;
    Context->Cash = DW_INITIAL_CASH;
    Context->WeaponCount = DW_INITIAL_WEAPON_COUNT;
    Context->WeaponDamage = DW_INITIAL_WEAPON_DAMAGE;
    Context->Health = DW_INITIAL_HEALTH;
    Context->Bank = DW_INITIAL_BANK;
    Context->Debt = DW_INITIAL_DEBT;
    Context->Space = DW_INITIAL_SPACE;
    Context->Day = DW_INITIAL_DAY;
    Context->Location = DW_INITIAL_LOCATION;
    for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
        Context->Inventory[Index] = 0;
    }

    DwDrawGameBoard(Context);
    return;
}

VOID
DwDrawGameBoard (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine draws the static and initial components of the game board.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Row;

    SwClearRegion(ConsoleColorGray, ConsoleColorDefault, 0, 0, 80, 25);
    SwClearRegion(ConsoleColorDarkBlue, ConsoleColorDefault, 1, 1, 78, 16);
    SwMoveCursor(stdout, 3, 0);
    SwPrintInColor(ConsoleColorGray,
                   ConsoleColorDarkBlue,
                   "%02d-01-19%02d",
                   DwRandom(1, 12),
                   DwRandom(60, 91));

    SwMoveCursor(stdout, 65, 0);
    SwPrintInColor(ConsoleColorGray, ConsoleColorDarkBlue, "Space    100");
    SwMoveCursor(stdout, 1, 1);
    SwPrintInColor(ConsoleColorDarkBlue,
                   ConsoleColorGray,
                   DwString(DwsHorizontalLine));

    SwMoveCursor(stdout, 1, 2);
    SwPrintInColor(ConsoleColorDarkBlue,
                   ConsoleColorGray,
                   DwString(DwsColumnTitles));

    SwMoveCursor(stdout, 1, 3);
    SwPrintInColor(ConsoleColorDarkBlue,
                   ConsoleColorGray,
                   DwString(DwsHorizontalLine));

    for (Row = 4; Row < 16; Row += 1) {
        SwMoveCursor(stdout, 1, Row);
        SwPrintInColor(ConsoleColorDarkBlue,
                       ConsoleColorGray,
                       DwString(DwsTwoColumnLine));
    }

    SwMoveCursor(stdout, 1, 16);
    SwPrintInColor(ConsoleColorDarkBlue,
                   ConsoleColorGray,
                   DwString(DwsHorizontalLine));

    DwRedrawCash(Context);
    DwRedrawWeapons(Context);
    DwRedrawHealth(Context);
    DwRedrawBank(Context);
    DwRedrawDebt(Context);
    return;
}

VOID
DwPlay (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine implements the main game loop.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    //
    // Live day by day.
    //

    while ((Context->Health > 0) && (Context->Day <= DW_GAME_TIME) &&
           (Context->ExitRequested == FALSE)) {

        SwMoveCursor(stdout, 6, 0);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorDarkBlue,
                       "%02d",
                       Context->Day);

        DwDrawLocation(DwLocationName(Context->Location));
        DwDoDailyEvents(Context);
        if (Context->Health <= 0) {
            break;
        }

        DwParticipateInMarket(Context);
        Context->Day += 1;
    }

    return;
}

VOID
DwDoDailyEvents (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs the daily routine, such as fetching market prices,
    and meeting people on the subway.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Chance;
    INT Index;
    CHAR Line[240];
    INT Price;
    PSTR Qualifier;
    PSTR SurgeFormat;
    INT Worth;

    //
    // Go through the regular chores every day except the first.
    //

    if (Context->Day != 1) {

        //
        // Update the loan shark and bank on another day.
        //

        if (Context->Debt != 0) {
            Context->Debt =
                         (Context->Debt * (DW_LOAN_INTEREST_RATE + 100)) / 100;

            DwRedrawDebt(Context);
        }

        if (Context->Bank != 0) {
            Context->Bank =
                         (Context->Bank * (DW_BANK_INTEREST_RATE + 100)) / 100;

            DwRedrawBank(Context);
        }

        //
        // Determine if something interesting is going to happen.
        //

        Worth = Context->Cash - Context->Debt;
        Chance = 100;
        if (Worth > 3000000) {
            Chance = 130;

        } else if (Worth > 1000000) {
            Chance = 115;
        }

        if (DwRandom(0, Chance) > 75) {
            Chance = 80 + DwLocations[Context->Location].PolicePresence;
            Chance = DwRandom(0, Chance);
            if (Chance < 33) {
                DwReceiveOffer(Context);

            } else if (Chance < 50) {
                DwPerformActOfGod(Context);

            } else {
                DwEncounterPolice(Context);
            }
        }

        if (Context->Health <= 0) {
            return;
        }

        //
        // Sometimes the lady on the subway pipes up.
        //

        if (DwRandom(0, 100) < 15) {
            if (DwRandom(0, 100) < 50) {
                Qualifier = "";
                if (DwRandom(0, 100) < 30) {
                    Qualifier = DwString(DwsSubwayQualifier);
                }

                Index = DwRandom(0, DW_SUBWAY_SAYINGS_COUNT);
                snprintf(Line,
                         sizeof(Line),
                         DwString(DwsSubwayLadyFormat),
                         DwString(DwsSubwaySayings + Index),
                         Qualifier);

            } else {
                Index = DwRandom(0, DW_SONG_COUNT);
                snprintf(Line,
                         sizeof(Line),
                         DwString(DwsHearSongFormat),
                         DwString(DwsSongs + Index));
            }
        }

        //
        // In a certain part of town, it's possible to visit some special folks.
        //

        if (Context->Location == DW_FINANCIAL_DISTRICT) {
            DwVisitFinancialDistrict(Context);
        }
    }

    //
    // Go get today's market prices, and note any large price fluctuations.
    //

    DwGenerateMarket(Context);
    for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
        Price = Context->Market[Index];

        //
        // If the good is not in the market or has a normal price, then ignore
        // it.
        //

        if ((Price == 0) ||
            ((Price >= DwGoods[Index].MinPrice) &&
             (Price < DwGoods[Index].MaxPrice))) {

            continue;
        }

        //
        // Everyone loves a sale.
        //

        if (Price < DwGoods[Index].MinPrice) {
            DwPresentNotification(Context, DwString(DwsGoodsSales + Index));

        //
        // A surge: help a brother out.
        //

        } else {
            if (DwRandom(0, 100) < 50) {
                SurgeFormat = DwString(DwsSurgeFormat1);

            } else {
                SurgeFormat = DwString(DwsSurgeFormat2);
            }

            snprintf(Line, sizeof(Line), SurgeFormat, DwGoodName(Index));
            DwPresentNotification(Context, Line);
        }
    }

    return;
}

VOID
DwReceiveOffer (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine gives the player a special product offer.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Answer;
    CHAR Line[80];
    INT Price;
    INT Product;

    //
    // Potentially offer the player more space for goods.
    //

    if (DwRandom(0, 100) < 50) {
        Price = DwRandom(DW_MIN_SPACE_PRICE, DW_MAX_SPACE_PRICE);
        if (Price <= Context->Cash) {
            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsProductOfferFormat),
                     DwString(DwsProductMoreSpace),
                     Price);

            Answer = DwReadYesNoAnswer(Context, NULL, Line);
            if (Answer == 1) {
                Context->Space += DwRandom(1, 2) * DW_MORE_SPACE;
                Context->Cash -= Price;
                DwRedrawSpace(Context);
                DwRedrawCash(Context);
            }
        }

    //
    // Offer the player a weapon.
    //

    } else {
        Product = DwRandom(0, DW_WEAPON_COUNT);
        Price = DwWeapons[Product].Price;
        if ((Price <= Context->Cash) &&
            (Context->Space >= DwWeapons[Product].Space)) {

            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsProductOfferFormat),
                     DwWeaponName(Product),
                     DwWeapons[Product].Price);

            Answer = DwReadYesNoAnswer(Context, NULL, Line);
            if (Answer == 1) {
                Context->Space -= DwWeapons[Product].Space;
                Context->WeaponCount += 1;
                Context->WeaponDamage += DwWeapons[Product].Damage;
                Context->Cash -= Price;
                DwRedrawCash(Context);
                DwRedrawSpace(Context);
                DwRedrawWeapons(Context);
            }
        }
    }

    return;
}

VOID
DwPerformActOfGod (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs a non-interactive and random action on the user.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Action;
    INT Amount;
    INT Answer;
    INT Good;
    INT Index;
    CHAR Line[160];

    //
    // Some actions require a good that the player currently has in a certain
    // quantity.
    //

    Amount = DwRandom(3, 7);
    for (Index = 0; Index < 5; Index += 1) {
        Good = DwRandom(0, DW_GOOD_COUNT);
        if (Context->Inventory[Good] >= Amount) {
            break;
        }
    }

    if (Index == 5) {
        Good = -1;
    }

    Action = DwRandom(0, 100);

    //
    // Sometimes there are muggers in the subway.
    //

    if (Action < 10) {
        DwPresentNotification(Context, DwString(DwsMugged));
        Context->Cash = (Context->Cash * DwRandom(80, 95)) / 100;
        DwRedrawCash(Context);

    //
    // Sometimes gifts are given or received.
    //

    } else if (Action < 30) {
        if (Good == -1) {

            //
            // Players lose out if they don't have space to receive gifts.
            //

            if (Amount > Context->Space) {
                Amount = 0;
            }

            Good = DwRandom(0, DW_GOOD_COUNT);
            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsReceiveGiftFormat),
                     DwGoodName(Good));

            Context->Inventory[Good] += Amount;
            Context->Space -= Amount;

        //
        // Well heeled players give gifts to others.
        //

        } else {
            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsSendGiftFormat),
                     DwGoodName(Good));

            Context->Inventory[Good] -= Amount;
            Context->Space += Amount;
        }

        if (Amount != 0) {
            DwPresentNotification(Context, Line);
            DwRedrawSpace(Context);
            DwRedrawInventory(Context);
        }

    //
    // Sometimes people just lose things, or find things.
    //

    } else if (Action < 50) {
        if (Good != -1) {
            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsLostGoodsFormat),
                     DwRandom(3, 7));

            Context->Inventory[Good] -= Amount;
            Context->Space += Amount;

        } else {
            if (Amount > Context->Space) {
                Amount = 0;
            }

            Good = DwRandom(0, DW_GOOD_COUNT);
            snprintf(Line,
                     sizeof(Line),
                     DwString(DwsFoundGoodsFormat),
                     Amount,
                     DwGoodName(Good));

            Context->Inventory[Good] += Amount;
            Context->Space -= Amount;
        }

        if (Amount != 0) {
            DwPresentNotification(Context, Line);
            DwRedrawSpace(Context);
            DwRedrawInventory(Context);
        }

    //
    // Sometimes other people share your items.
    //

    } else if ((Action < 60) &&
               ((Context->Inventory[DW_BROWNIE_GOOD1] != 0) ||
                (Context->Inventory[DW_BROWNIE_GOOD2] != 0))) {

        Good = DW_BROWNIE_GOOD1;
        if (Context->Inventory[DW_BROWNIE_GOOD2] >
            Context->Inventory[DW_BROWNIE_GOOD1]) {

            Good = DW_BROWNIE_GOOD2;
        }

        Amount = DwRandom(2, 6);
        if (Amount > Context->Inventory[Good]) {
            Amount = Context->Inventory[Good];
        }

        snprintf(Line,
                 sizeof(Line),
                 DwString(DwsSharedGoodsFormat),
                 DwGoodName(Good));

        Context->Inventory[Good] -= Amount;
        Context->Space += Amount;
        DwPresentNotification(Context, Line);
        DwRedrawSpace(Context);
        DwRedrawInventory(Context);

    //
    // Sometimes there's that offer that's just too good to be true.
    //

    } else if (Action < 65) {
        Answer = DwReadYesNoAnswer(Context,
                                   DwString(DwsSirenSong),
                                   DwString(DwsSirenPrompt));

        if (Answer == 1) {
            SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, "Y");
            SwMoveCursor(stdout, 1, 21);
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsSirenResult));

            DwPresentNotification(Context, NULL);
            Context->Health = 0;
        }

    //
    // Occasionally the player just needs to take a break from the hustle to
    // sit and think.
    //

    } else {
        Index = DwRandom(0, DW_PASSIVE_ACTIVITY_COUNT);
        snprintf(Line,
                 sizeof(Line),
                 DwString(DwsPassiveActivityFormat),
                 DwString(DwsPassiveActivities + Index));

        DwPresentNotification(Context, Line);
        Amount = DwRandom(1, 10);
        if (Context->Cash >= Amount) {
            Context->Cash -= Amount;
        }

        DwRedrawCash(Context);
    }

    return;
}

VOID
DwEncounterPolice (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine is called when the player has a brush with the law.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Answer;
    PSTR Choices;
    INT Damage;
    INT Defence;
    INT DoctorCost;
    INT Enemies;
    INT FancyFeet;
    INT MaxEnemies;
    INT Offense;
    PSTR Prompt;
    INT Row;
    PSTR ShotResult;
    INT VictoryCash;

    //
    // How bad could this be?
    //

    MaxEnemies = 4;
    if (Context->Day >= 15) {
        MaxEnemies = 6;
        if (Context->Day > 23) {
            MaxEnemies = 9;
        }
    }

    Enemies = DwRandom(2, MaxEnemies);
    MaxEnemies = Enemies;

    //
    // Loop handlin' bad guys.
    //

    while (Enemies != 0) {
        DwClearLowerRegion();
        Row = 18;
        SwMoveCursor(stdout, 1, Row);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorBlack,
                       DwString(DwsFightThreatFormat),
                       Enemies - 1);

        Row += 1;
        SwMoveCursor(stdout, 1, Row);
        if (Context->WeaponCount != 0) {
            Prompt = DwString(DwsRunOrFight);
            Choices = "rf";

        } else {
            Prompt = DwString(DwsRunOption);
            Choices = "yn";
        }

        SwPrintInColor(ConsoleColorGray, ConsoleColorDarkMagenta, Prompt);
        Answer = DwReadCharacterSet(Context, Choices);
        if (Answer == EOF) {
            return;
        }

        //
        // Fight on, player.
        //

        if (Answer == 'f') {
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsFight));

            Row += 1;
            DwFlashText(Context,
                        DwString(DwsPlayerFire),
                        1,
                        Row,
                        ConsoleColorGray,
                        ConsoleColorBlack,
                        ConsoleColorWhite,
                        FALSE);

            Offense = 50 + Context->WeaponDamage;
            ShotResult = DwString(DwsPlayerMissed);
            if (DwRandom(0, 100) < Offense) {
                Enemies -= 1;
                ShotResult = DwString(DwsPlayerHit);
            }

            SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, ShotResult);

        //
        // Flee from the police!
        //

        } else if ((Answer == 'y') || (Answer == 'r')) {
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsRun));

            Row += 1;
            SwMoveCursor(stdout, 1, Row);
            FancyFeet = 65 - (Enemies * 5);
            if (DwRandom(0, 100) < FancyFeet) {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               DwString(DwsFled));

                DwPresentNotification(Context, NULL);
                break;

            } else {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               DwString(DwsFailedToFlee));
            }

        //
        // Just stand around and hope nothing happens.
        //

        } else {
            Row += 1;
            SwMoveCursor(stdout, 1, Row);
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsNotFleeing));
        }

        Row += 1;

        //
        // If there are still enemies, they fire at our hero.
        //

        if (Enemies != 0) {
            SwMoveCursor(stdout, 1, Row);
            DwFlashText(Context,
                        DwString(DwsPlayerUnderFire),
                        1,
                        Row,
                        ConsoleColorGray,
                        ConsoleColorBlack,
                        ConsoleColorWhite,
                        FALSE);

            Defence = 60 - (Enemies * 5);
            if (DwRandom(0, 100) < Defence) {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               DwString(DwsTheyMissed));

            } else {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               DwString(DwsTheyHit));

                Damage = DwRandom(5, 10);
                if (Damage >= Context->Health) {
                    Context->Health = 0;

                } else {
                    Context->Health -= Damage;
                }
            }

            DwRedrawHighlightedHealth(Context);
            if (Context->Health <= 0) {
                Row += 1;
                SwMoveCursor(stdout, 1, Row);
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               DwString(DwsKilled));
            }

            DwPresentNotification(Context, NULL);
            DwRedrawHealth(Context);
            if (Context->Health <= 0) {
                break;
            }

        //
        // Victory!
        //

        } else {
            DwPresentNotification(Context, NULL);
            SwClearRegion(ConsoleColorGray, ConsoleColorDefault, 0, 24, 80, 1);
            VictoryCash = DwRandom(1000, 1500 + (MaxEnemies * 200));
            SwMoveCursor(stdout, 1, Row);
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsFightVictoryFormat),
                           VictoryCash);

            Context->Cash += VictoryCash;
            DoctorCost = DwRandom(100, 200) *
                         (DW_INITIAL_HEALTH - Context->Health) / 5;

            if (DoctorCost > Context->Cash) {
                DoctorCost = Context->Cash;
            }

            if (DoctorCost != 0) {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorDarkMagenta,
                               DwString(DwsDoctorOffer),
                               DoctorCost);

                Answer = DwReadYesNoAnswer(Context, NULL, NULL);
                if (Answer == EOF) {
                    return;
                }

                if (Answer == 1) {
                    Context->Cash -= DoctorCost;
                    Context->Health = DW_INITIAL_HEALTH;
                }

            } else {
                DwPresentNotification(Context, NULL);
            }

            DwRedrawCash(Context);
            DwRedrawHealth(Context);
            break;
        }
    }

    return;
}

VOID
DwVisitFinancialDistrict (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine is called to prompt the user and handle visits to the bank and
    potentially the loan shark.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Action;
    INT Answer;
    INT Value;

    //
    // Visit the loan shark if the player has a loan.
    //

    if (Context->Debt != 0) {
        Answer = DwReadYesNoAnswer(Context, NULL, DwString(DwsVisitLoanShark));
        if (Answer == -1) {
            return;
        }

        if (Answer == 1) {
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwString(DwsYes));

            SwMoveCursor(stdout, 1, 20);
            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorDarkMagenta,
                           DwString(DwsLoanRepaymentAmount));

            Value = DwReadQuantity(Context);
            if (Value > 0) {
                if ((Value <= Context->Cash) && (Value <= Context->Debt)) {
                    Context->Cash -= Value;
                    Context->Debt -= Value;
                    DwRedrawCash(Context);
                    if (Context->Debt == 0) {
                        SwClearRegion(ConsoleColorDarkBlue,
                                      ConsoleColorDefault,
                                      9,
                                      13,
                                      20,
                                      1);

                    } else {
                        DwRedrawDebt(Context);
                    }
                }
            }
        }
    }

    //
    // Visit the bank.
    //

    Answer = DwReadYesNoAnswer(Context, NULL, DwString(DwsVisitBank));
    if (Answer == -1) {
        return;
    }

    if (Answer == 1) {
        SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, DwString(DwsYes));
        SwMoveCursor(stdout, 1, 19);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorDarkMagenta,
                       DwString(DwsDepositOrWithdraw));

        Action = DwReadCharacterSet(Context, "dw");
        if (Action == -1) {
            return;
        }

        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorBlack,
                       "%c",
                       toupper(Action));

        SwMoveCursor(stdout, 1, 20);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorDarkMagenta,
                       DwString(DwsHowMuchMoney));

        Value = DwReadQuantity(Context);
        if (Value > 0) {
            if (Action == 'd') {
                if (Value <= Context->Cash) {
                    Context->Cash -= Value;
                    Context->Bank += Value;
                }

            } else if (Action == 'w') {
                if (Value <= Context->Bank) {
                    Context->Cash += Value;
                    Context->Bank -= Value;
                }
            }

            DwRedrawBank(Context);
            DwRedrawCash(Context);
        }
    }

    return;
}

VOID
DwGenerateMarket (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates a new market's worth of goods and prices.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT EventCount;
    INT GoodsCount;
    INT Index;
    INT TotalGoodsCount;

    for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
        Context->Market[Index] = 0;
    }

    //
    // Determine how many special events will occur today.
    //

    EventCount = 0;
    if (DwRandom(0, 100) < 70) {
        EventCount = 1;
        if (DwRandom(0, 100) < 40) {
            EventCount = 2;
            if (DwRandom(0, 100) < 5) {
                EventCount = 3;
            }
        }
    }

    //
    // Determine what those special events are.
    //

    GoodsCount = 0;
    while (EventCount > 0) {
        Index = DwRandom(0, DW_GOOD_COUNT);
        if (Context->Market[Index] != 0) {
            continue;
        }

        if ((DwGoods[Index].Surges == FALSE) &&
            (DwGoods[Index].Sales == FALSE)) {

            continue;
        }

        GoodsCount += 1;
        EventCount -= 1;
        Context->Market[Index] = DwRandom(DwGoods[Index].MinPrice,
                                          DwGoods[Index].MaxPrice);

        if (DwGoods[Index].Surges != FALSE) {
            Context->Market[Index] *= DW_SURGE_FACTOR;

        } else if (DwGoods[Index].Sales != FALSE) {
            Context->Market[Index] /= DW_SALE_FACTOR;
        }
    }

    //
    // Determine how many goods will be in the market.
    //

    TotalGoodsCount = DwRandom(DwLocations[Context->Location].MinGoods,
                               DwLocations[Context->Location].MaxGoods);

    assert(TotalGoodsCount <= DW_GOOD_COUNT);

    GoodsCount = TotalGoodsCount - GoodsCount;
    while (GoodsCount > 0) {
        Index = DwRandom(0, DW_GOOD_COUNT);
        if (Context->Market[Index] != 0) {
            continue;
        }

        Context->Market[Index] = DwRandom(DwGoods[Index].MinPrice,
                                          DwGoods[Index].MaxPrice);

        GoodsCount -= 1;
    }

    return;
}

VOID
DwParticipateInMarket (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine is where the player buys and sells goods.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Action;
    INT Choice;
    CHAR ChoiceBuffer[DW_GOOD_COUNT + 1];
    PSTR Choices;
    INT Index;
    INT MaxQuantity;
    PSTR Prompt;
    INT Quantity;
    INT SelectedGood;

    while (Context->ExitRequested == FALSE) {
        DwDrawMarket(Context);

        //
        // Determine if the user has anything in inventory, and can therefore
        // sell.
        //

        Choices = "bj";
        Prompt = DwString(DwsBuyOrJet);
        for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
            if (Context->Inventory[Index] != 0) {
                Choices = "bsj";
                Prompt = DwString(DwsBuySellJet);
                break;
            }
        }

        DwDrawBottomPrompt(Prompt);
        Action = DwReadCharacterSet(Context, Choices);
        if (Action == EOF) {
            break;
        }

        if ((Action == 'b') || (Action == 's')) {
            SwClearRegion(ConsoleColorGray, ConsoleColorDefault, 0, 24, 80, 1);
            SwMoveCursor(stdout, 1, 23);
            if (Action == 'b') {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorDarkMagenta,
                               DwString(DwsWhatToBuy));

            } else {
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorDarkMagenta,
                               DwString(DwsWhatToSell));
            }

            //
            // Create the set of possibilities. For buying, anything in the
            // market is a valid choice. For selling, anything in inventory
            // is a valid choice.
            //

            Choice = 'a';
            Choices = ChoiceBuffer;
            for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
                if (Context->Market[Index] != 0) {
                    if ((Action == 'b') || (Context->Inventory[Index] != 0)) {
                        *Choices = Choice;
                        Choices += 1;
                    }

                    Choice += 1;
                }
            }

            *Choices = '\0';
            Choice = DwReadCharacterSet(Context, ChoiceBuffer);
            if (Choice == EOF) {
                break;
            }

            //
            // Translate back to figure out which good they were talking about.
            //

            Choice -= 'a';
            SelectedGood = 0;
            for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
                if (Context->Market[Index] != 0) {
                    if (Choice == SelectedGood) {
                        SelectedGood = Index;
                        break;
                    }

                    SelectedGood += 1;
                }
            }

            SwPrintInColor(ConsoleColorGray,
                           ConsoleColorBlack,
                           DwGoodName(SelectedGood));

            //
            // Ask how many they'd like to purchase or sell.
            //

            SwMoveCursor(stdout, 1, 24);
            if (Action == 'b') {
                MaxQuantity = Context->Cash / Context->Market[Index];
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorDarkMagenta,
                               DwString(DwsHowManyToBuy),
                               MaxQuantity);

            } else {
                MaxQuantity = Context->Inventory[Index];
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorDarkMagenta,
                               DwString(DwsHowManyToSell),
                               MaxQuantity);
            }

            Quantity = DwReadQuantity(Context);

            //
            // Do the deal. The check for space is only needed when buying.
            //

            if ((Quantity > 0) && (Quantity <= MaxQuantity) &&
                ((Action == 's') || (Quantity <= Context->Space))) {

                //
                // Selling is really just buying negative quantities. Chew on
                // that.
                //

                if (Action == 's') {
                    Quantity = -Quantity;
                }

                Context->Inventory[SelectedGood] += Quantity;
                Context->Cash -= Quantity * Context->Market[SelectedGood];
                Context->Space -= Quantity;
                DwRedrawCash(Context);
                DwRedrawInventory(Context);
                DwRedrawSpace(Context);
            }

        //
        // They're outta here.
        //

        } else if (Action == 'j') {
            DwClearLowerRegion();
            for (Index = 0; Index < DW_LOCATION_COUNT; Index += 1) {
                SwMoveCursor(stdout, 4 + ((Index % 3) * 28), 18 + (Index / 3));
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               "%d.  %s",
                               Index + 1,
                               DwLocationName(Index));
            }

            DwDrawBottomPrompt(DwString(DwsWhereTo));
            Choice = DwReadCharacterSet(Context, "123456");
            if (Choice == EOF) {
                return;
            }

            //
            // If they want a different place than they are, then ride the
            // subway away from here.
            //

            Choice -= '1';
            if (Choice != Context->Location) {

                assert(Choice < DW_LOCATION_COUNT);

                Context->Location = Choice;
                DwClearLowerRegion();

                //
                // Ride the subway.
                //

                DwDrawLocation("");
                DwFlashText(Context,
                            DwString(DwsSubway),
                            33,
                            2,
                            ConsoleColorDarkBlue,
                            ConsoleColorWhite,
                            ConsoleColorDarkBlue,
                            TRUE);

                DwDrawLocation(DwLocationName(Choice));
                break;
            }
        }
    }

    return;
}

VOID
DwDrawMarket (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine redraws the current daily market contents.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT ColumnIndex;
    INT Index;
    CHAR Line[16];
    CHAR Price[11];
    INT Row;
    CHAR Selector;

    DwClearLowerRegion();
    Selector = 'A';
    SwMoveCursor(stdout, 1, 18);
    SwPrintInColor(ConsoleColorGray,
                   ConsoleColorBlack,
                   DwString(DwsMarketGreeting));

    Row = 19;
    ColumnIndex = 0;
    for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
        if (Context->Market[Index] == 0) {
            continue;
        }

        SwMoveCursor(stdout, 4 + (ColumnIndex * 26), Row);
        snprintf(Line, sizeof(Line), "%c> %s", Selector, DwGoodName(Index));
        SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, Line);
        DwFormatMoney(Price, sizeof(Price), Context->Market[Index]);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorBlack,
                       "%*s",
                       20 - strlen(Line),
                       Price);

        Selector += 1;
        if (ColumnIndex == 2) {
            ColumnIndex = 0;
            Row += 1;

        } else {
            ColumnIndex += 1;
        }
    }

    return;
}

VOID
DwRedrawInventory (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine redraws the player's current inventory.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INT Index;
    INT Quantity;
    INT Row;

    SwClearRegion(ConsoleColorDarkBlue,
                  ConsoleColorDefault,
                  47,
                  4,
                  24,
                  DW_GOOD_COUNT);

    Row = 4;
    for (Index = 0; Index < DW_GOOD_COUNT; Index += 1) {
        Quantity = Context->Inventory[Index];
        if (Quantity == 0) {
            continue;
        }

        SwMoveCursor(stdout, 47, Row);
        SwPrintInColor(ConsoleColorDarkBlue,
                       ConsoleColorGray,
                       DwGoodName(Index));

        SwMoveCursor(stdout, 47 + 18, Row);
        SwPrintInColor(ConsoleColorDarkBlue,
                       ConsoleColorGray,
                       "%6d",
                       Quantity);

        Row += 1;
    }

    return;
}

VOID
DwPresentNotification (
    PDW_CONTEXT Context,
    PSTR Notification
    )

/*++

Routine Description:

    This routine presents a notification to the user in the bottom area, and
    waits for them to press space.

Arguments:

    Context - Supplies a pointer to the application context.

    Notification - Supplies an pointer to the notification. If NULL, then the
        lower screen isn't cleared, as it's assumed the notification is already
        set up.

Return Value:

    None.

--*/

{

    if (Notification != NULL) {
        DwClearLowerRegion();
        SwMoveCursor(stdout, 1, 18);
        SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, Notification);
    }

    DwDrawBottomPrompt(DwString(DwsPressSpace));
    DwReadCharacterSet(Context, " ");
    return;
}

VOID
DwDrawBottomPrompt (
    PSTR Prompt
    )

/*++

Routine Description:

    This routine prints a string centered in purple at the bottom of the
    screen. This routine assumes the bottom line is already clear.

Arguments:

    Prompt - Supplies a pointer to the string containing the prompt to print.

Return Value:

    None.

--*/

{

    UINTN Column;
    UINTN Length;

    Length = strlen(Prompt);
    if (Length >= 80) {
        Column = 0;

    } else {
        Column = 40 - (Length / 2);
    }

    SwMoveCursor(stdout, Column, 24);
    SwPrintInColor(ConsoleColorGray, ConsoleColorDarkMagenta, Prompt);
    return;
}

VOID
DwDrawStat (
    PSTR Name,
    INT Row,
    INT Value,
    BOOL Money,
    CONSOLE_COLOR Foreground,
    CONSOLE_COLOR Background
    )

/*++

Routine Description:

    This routine redraws one of the statistics on the left pane.

Arguments:

    Name - Supplies a pointer to the name of the statistic.

    Row - Supplies the starting row of the statistic.

    Value - Supplies the value of the stat.

    Money - Supplies a boolean indicating if this is a monetary value or not.

    Foreground - Supplies the text color to use.

    Background - Supplies the background color to use.

Return Value:

    None.

--*/

{

    CHAR Line[21];
    CHAR ValueString[21];

    if (Money != FALSE) {
        DwFormatMoney(ValueString, sizeof(ValueString), Value);

    } else {
        snprintf(ValueString, sizeof(ValueString), "%d", Value);
    }

    snprintf(Line,
             sizeof(Line),
             "%s%*s",
             Name,
             (int)(20 - strlen(Name)),
             ValueString);

    SwMoveCursor(stdout, 9, Row);
    SwPrintInColor(Background, Foreground, Line);
    return;
}

VOID
DwDrawLocation (
    PSTR Location
    )

/*++

Routine Description:

    This routine redraws the current location.

Arguments:

    Location - Supplies the name of the location.

Return Value:

    None.

--*/

{

    INT Column;
    INT Length;

    SwClearRegion(ConsoleColorDarkBlue, ConsoleColorGray, 33, 2, 13, 1);

    //
    // Center the text.
    //

    Length = strlen(Location);
    if (Length <= 14) {
        Column = 39 - (Length / 2);

    } else {
        Column = 33;
    }

    SwMoveCursor(stdout, Column, 2);
    SwPrintInColor(ConsoleColorDarkBlue, ConsoleColorWhite, Location);
    return;
}

VOID
DwRedrawSpace (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine redraws the players remaining inventory capacity.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    SwMoveCursor(stdout, 73, 0);
    SwPrintInColor(ConsoleColorGray,
                   ConsoleColorDarkBlue,
                   "%4d",
                   Context->Space);

    return;
}

VOID
DwFlashText (
    PDW_CONTEXT Context,
    PSTR String,
    INT XPosition,
    INT YPosition,
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    CONSOLE_COLOR FlashForeground,
    BOOL Fast
    )

/*++

Routine Description:

    This routine flashes text for a little while, for an exciting visual
    experience.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies a pointer to the string to flash.

    XPosition - Supplies the column to draw the text at.

    YPosition - Supplies the row to draw the text at.

    Background - Supplies the background color.

    Foreground - Supplies the initial and final foreground color.

    FlashForeground - Supplies the accent foreground color.

    Fast - Supplies a boolean indicating whether to flash fast or slow.

Return Value:

    None.

--*/

{

    INT Count;
    CONSOLE_COLOR CurrentColor;
    INT Delay;
    INT Index;

    if (Fast != FALSE) {
        Delay = DW_FLASH_FAST_MICROSECONDS;

    } else {
        Delay = DW_FLASH_SLOW_MICROSECONDS;
    }

    Count = DwRandom(5, 7) | 1;
    for (Index = 0; Index < Count; Index += 1) {
        SwMoveCursor(stdout, XPosition, YPosition);
        if ((Index & 0x1) != 0) {
            CurrentColor = FlashForeground;

        } else {
            CurrentColor = Foreground;
        }

        SwPrintInColor(Background, CurrentColor, "%s", String);
        SwSleep(Delay);
    }

    return;
}

INT
DwDisplayHighScores (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine displays the "high" scores. Oh yeah.

Arguments:

    Context - Supplies a pointer to the player context.

Return Value:

    0 if the caller does not want to play again.

    1 if the caller does want to play again (yay).

    -1 if the answer could not be read.

--*/

{

    CHAR Amount[16];
    INT Answer;
    CONSOLE_COLOR Color;
    struct tm *CurrentTime;
    PSTR Dead;
    PDW_HIGH_SCORE_ENTRY Entry;
    INT Index;
    CHAR Line[81];
    PSTR Name;
    INT PlayerValue;
    INT Result;
    INT Row;
    BOOL Save;
    DW_HIGH_SCORES Scores;
    time_t Time;

    Save = FALSE;
    DwLoadHighScores(Context, &Scores);

    //
    // If the player made the high score list, ask for their name and remember
    // to save the high scores list.
    //

    PlayerValue = Context->Cash + Context->Bank - Context->Debt;
    Entry = &(Scores.Entries[DW_HIGH_SCORE_COUNT - 1]);
    Name = (PSTR)(Entry->Name);
    if (((Entry->Flags & DW_HIGH_SCORE_VALID) == 0) ||
        (PlayerValue > Entry->Amount)) {

        DwClearLowerRegion();
        SwMoveCursor(stdout, 1, 18);
        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorBlack,
                       DwString(DwsMadeHighScores));

        SwPrintInColor(ConsoleColorGray,
                       ConsoleColorDarkMagenta,
                       DwString(DwsNamePrompt));

        Result = DwReadString(Context, Name, DW_HIGH_SCORE_NAME_SIZE);
        if (Result != 0) {
            strncpy(Name, DwString(DwsAnonymous), DW_HIGH_SCORE_NAME_SIZE);
        }

        Save = TRUE;

    } else {
        strncpy(Name, DwString(DwsYou), DW_HIGH_SCORE_NAME_SIZE);
    }

    //
    // Add the remainder of the player's details, always to the last score.
    //

    Entry->Flags = DW_HIGH_SCORE_VALID | DW_HIGH_SCORE_YOU;
    if (Context->Health != 0) {
        Entry->Flags |= DW_HIGH_SCORE_ALIVE;
    }

    Entry->Amount = PlayerValue;
    Time = time(NULL);
    CurrentTime = localtime(&Time);
    if (CurrentTime != NULL) {
        Entry->Month = CurrentTime->tm_mon + 1;
        Entry->Day = CurrentTime->tm_mday;
        Entry->Year = CurrentTime->tm_year + 1900;
    }

    //
    // Sort the scores now.
    //

    qsort(&(Scores.Entries[0]),
          DW_HIGH_SCORE_COUNT,
          sizeof(DW_HIGH_SCORE_ENTRY),
          DwCompareHighScores);

    //
    // Print the high scores screen.
    //

    SwClearRegion(ConsoleColorDarkBlue, ConsoleColorDefault, 0, 0, 80, 25);
    SwMoveCursor(stdout, 28, 0);
    SwPrintInColor(ConsoleColorDarkBlue,
                   ConsoleColorYellow,
                   DwString(DwsHighScoresTitle));

    Row = 4;
    for (Index = 0; Index < DW_HIGH_SCORE_COUNT; Index += 1) {
        Entry = &(Scores.Entries[Index]);
        if ((Entry->Flags & DW_HIGH_SCORE_VALID) == 0) {
            continue;
        }

        DwFormatMoney(Amount, sizeof(Amount), Entry->Amount);
        Entry->Name[DW_HIGH_SCORE_NAME_SIZE - 1] = '\0';
        Dead = "";
        if ((Entry->Flags & DW_HIGH_SCORE_ALIVE) == 0) {
            Dead = DwString(DwsHighScoreDead);
        }

        snprintf(Line,
                 sizeof(Line),
                 DwString(DwsHighScoreFormat),
                 Amount,
                 Entry->Month,
                 Entry->Day,
                 Entry->Year,
                 Entry->Name,
                 Dead);

        Color = ConsoleColorGray;
        if ((Entry->Flags & DW_HIGH_SCORE_YOU) != 0) {
            Color = ConsoleColorWhite;
            Entry->Flags &= ~DW_HIGH_SCORE_YOU;
        }

        SwMoveCursor(stdout, 6, Row);
        SwPrintInColor(ConsoleColorDarkBlue, Color, "%s", Line);
        Row += 1;
    }

    //
    // Save the high scores file.
    //

    if (Save != FALSE) {
        Scores.Checksum = 0;
        Scores.Checksum = DwChecksum(&Scores, sizeof(DW_HIGH_SCORES));
        DwHighScoresFileIo(&Scores, FALSE);
    }

    //
    // Let's do it all again!
    //

    DwDrawBottomPrompt(DwString(DwsPlayAgain));
    Answer = DwReadYesNoAnswer(Context, NULL, NULL);
    return Answer;
}

VOID
DwLoadHighScores (
    PDW_CONTEXT Context,
    PDW_HIGH_SCORES Scores
    )

/*++

Routine Description:

    This routine loads the high scores, or initializes a new set.

Arguments:

    Context - Supplies a pointer to the player context.

    Scores - Supplies a pointer where the loaded stores will be returned.

Return Value:

    None. If high scores could not be read, new ones will be initialized.

--*/

{

    ULONG Checksum;
    INT Result;

    //
    // Try to load the high scores and validate the CRC32.
    //

    Result = DwHighScoresFileIo(Scores, TRUE);
    if ((Result == 0) && (Scores->Magic == DW_HIGH_SCORE_MAGIC)) {
        Checksum = Scores->Checksum;
        Scores->Checksum = 0;
        if (DwChecksum(Scores, sizeof(DW_HIGH_SCORES)) == Checksum) {
            Scores->Checksum = Checksum;
            return;
        }
    }

    //
    // Either the scores didn't load or the CRC didn't match, so initialize a
    // new set.
    //

    memset(Scores, 0, sizeof(DW_HIGH_SCORES));
    Scores->Magic = DW_HIGH_SCORE_MAGIC;
    return;
}

INT
DwHighScoresFileIo (
    PDW_HIGH_SCORES Scores,
    BOOL Load
    )

/*++

Routine Description:

    This routine attempts to load or save the high scores file.

Arguments:

    Scores - Supplies a pointer to the high scores structure.

    Load - Supplies a boolean indicating whether to load (TRUE) or store
        (FALSE) the file.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Access;
    PUCHAR Bytes;
    FILE *File;
    PSTR Home;
    INT Index;
    UCHAR Pad;
    CHAR Path[256];
    ssize_t Size;

    Home = getenv("HOME");
    if (Home == NULL) {
        Home = ".";
    }

    snprintf(Path, sizeof(Path), "%s/.dwsco", Home);
    Access = "wb";
    if (Load != FALSE) {
        Access = "rb";
    }

    File = fopen(Path, Access);
    if (File == NULL) {
        return errno;
    }

    Bytes = (PUCHAR)Scores;
    if (Load != FALSE) {
        Size = fread(Scores, 1, sizeof(DW_HIGH_SCORES), File);

        //
        // When the stakes are this high it's important to have truly
        // bulletproof security.
        //

        Pad = 0x56;
        for (Index = 0; Index < sizeof(DW_HIGH_SCORES); Index += 1) {
            Bytes[Index] ^= Pad;
            Pad += 1;
        }

    } else {
        Pad = 0x56;
        for (Index = 0; Index < sizeof(DW_HIGH_SCORES); Index += 1) {
            Bytes[Index] ^= Pad;
            Pad += 1;
        }

        Size = fwrite(Scores, 1, sizeof(DW_HIGH_SCORES), File);
    }

    fclose(File);
    if (Size != sizeof(DW_HIGH_SCORES)) {
        if (errno == 0) {
            return -1;
        }

        return errno;
    }

    return 0;
}

int
DwCompareHighScores (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares two high score entries.

Arguments:

    LeftPointer - Supplies the left high score pointer to compare.

    RightPointer - Supplies the right pointer to compare.

Return Value:

    <0 if the left is less than the right.

    0 if the left is equal to the right.

    >0 if the left is greater than the right.

--*/

{

    PDW_HIGH_SCORE_ENTRY Left;
    LONG LeftValue;
    PDW_HIGH_SCORE_ENTRY Right;
    LONG RightValue;

    Left = (PDW_HIGH_SCORE_ENTRY)LeftPointer;
    LeftValue = Left->Amount;
    Right = (PDW_HIGH_SCORE_ENTRY)RightPointer;
    RightValue = Right->Amount;

    //
    // First compare invalid entries.
    //

    if ((Left->Flags & DW_HIGH_SCORE_VALID) == 0) {
        LeftValue = -999999999;
    }

    if ((Right->Flags & DW_HIGH_SCORE_VALID) == 0) {
        RightValue = -999999999;
    }

    //
    // Higher scores should be first in the list.
    //

    if (LeftValue < RightValue) {
        return 1;

    } else if (LeftValue > RightValue) {
        return -1;
    }

    return 0;
}

VOID
DwFormatMoney (
    PSTR String,
    UINTN StringSize,
    INT Value
    )

/*++

Routine Description:

    This routine prints a money value, with thousands separators.

Arguments:

    String - Supplies a pointer to the output buffer.

    StringSize - Supplies the size of the output buffer string in bytes.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    INT Billions;
    INT Millions;
    INT Ones;
    INT Thousands;

    if (Value < 0) {
        Value = -Value;
        *String = '-';
        String += 1;
        StringSize -= 1;
    }

    Billions = Value / 1000000000;
    Ones = Value % 1000000000;
    Millions = Ones / 1000000;
    Ones = Ones % 1000000;
    Thousands = Ones / 1000;
    Ones = Ones % 1000;
    if (Billions != 0) {
        snprintf(String,
                 StringSize,
                 "$%d,%03d,%03d,%03d",
                 Billions,
                 Millions,
                 Thousands,
                 Ones);

    } else if (Millions != 0) {
        snprintf(String,
                 StringSize,
                 "$%d,%03d,%03d",
                 Millions,
                 Thousands,
                 Ones);

    } else if (Thousands != 0) {
        snprintf(String, StringSize, "$%d,%03d", Thousands, Ones);

    } else {
        snprintf(String, StringSize, "$%d", Ones);
    }

    return;
}

INT
DwReadCharacterSet (
    PDW_CONTEXT Context,
    PSTR Set
    )

/*++

Routine Description:

    This routine reads a character from a given permissible set.

Arguments:

    Context - Supplies a pointer to the application context.

    Set - Supplies a pointer to a null terminated string containing the set of
        acceptable characters.

Return Value:

    Returns the character received on success.

    -1 on read failure.

--*/

{

    INT Character;
    PSTR Current;

    while (TRUE) {
        Character = DwReadCharacter(Context);
        if (Character == -1) {
            break;
        }

        if (isupper(Character)) {
            Character = tolower(Character);
        }

        //
        // See if the character received was in the allowed set.
        //

        Current = Set;
        while (*Current != '\0') {
            if (*Current == Character) {
                return Character;
            }

            Current += 1;
        }
    }

    return Character;
}

INT
DwReadQuantity (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads a numeric quantity in from standard input.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the numeric value read in on success.

    -1 on failure.

--*/

{

    CHAR Buffer[9];
    INT Character;
    INT Index;
    INT ScanIndex;
    INT Value;

    Index = 0;
    while (TRUE) {
        Character = DwReadCharacter(Context);
        if (Character == EOF) {
            return -1;
        }

        if (isdigit(Character)) {
            if (Index < 8) {
                Buffer[Index] = Character;
                Index += 1;
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               "%c",
                               Character);
            }

        } else if ((Character == Context->Backspace) || (Character == '\b')) {
            if (Index > 0) {
                SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, "\b \b");
                Index -= 1;
            }

        } else if ((Character == '\r') || (Character == '\n')) {
            break;
        }
    }

    if (Index == 0) {
        return -1;
    }

    Value = 0;
    for (ScanIndex = 0; ScanIndex < Index; ScanIndex += 1) {
        Value = (Value * 10) + (Buffer[ScanIndex] - '0');
    }

    return Value;
}

INT
DwReadString (
    PDW_CONTEXT Context,
    PSTR String,
    INT StringSize
    )

/*++

Routine Description:

    This routine reads a generic string from standard input.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies a pointer where the string will be returned on success.

    StringSize - Supplies string buffer size.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    INT Character;
    INT Index;

    Index = 0;
    while (TRUE) {
        Character = DwReadCharacter(Context);
        if (Character == EOF) {
            return -1;
        }

        if ((Character == Context->Backspace) || (Character == '\b')) {
            if (Index > 0) {
                SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, "\b \b");
                Index -= 1;
            }

        } else if ((Character == '\r') || (Character == '\n')) {
            break;

        } else {
            if (Index < StringSize - 1) {
                String[Index] = Character;
                Index += 1;
                SwPrintInColor(ConsoleColorGray,
                               ConsoleColorBlack,
                               "%c",
                               Character);
            }
        }
    }

    if (Index == 0) {
        return -1;
    }

    String[Index] = '\0';
    return 0;
}

INT
DwReadYesNoAnswer (
    PDW_CONTEXT Context,
    PSTR Exposition,
    PSTR Prompt
    )

/*++

Routine Description:

    This routine reads a yes/no answer from the user.

Arguments:

    Context - Supplies a pointer to the application context.

    Exposition - Supplies an optional pointer to a line of black text to put
        first. Most questions don't have this.

    Prompt - Supplies an optional pointer to the question to ask.

Return Value:

    0 if the user said no.

    1 if the user said yes.

    -1 on failure.

--*/

{

    INT Answer;
    INT Row;

    if (Prompt != NULL) {
        DwClearLowerRegion();
    }

    Row = 18;
    if (Exposition != NULL) {
        SwMoveCursor(stdout, 1, Row);
        Row = 19;
        SwPrintInColor(ConsoleColorGray, ConsoleColorBlack, Exposition);
    }

    if (Prompt != NULL) {
        SwMoveCursor(stdout, 1, Row);
        SwPrintInColor(ConsoleColorGray, ConsoleColorDarkMagenta, Prompt);
    }

    Answer = DwReadCharacterSet(Context, "yn");
    if (Answer == EOF) {
        return Answer;
    }

    if (Answer == 'y') {
        return 1;
    }

    return 0;
}

INT
DwReadCharacter (
    PDW_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads a key from standard in.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the character received on success.

    EOF on read failure or end of input.

--*/

{

    INT Character;

    Character = SwReadInputCharacter();
    if (Character == 0x3) {
        Context->ExitRequested = TRUE;
        return -1;
    }

    return Character;
}

INT
DwRandom (
    INT Minimum,
    INT Maximum
    )

/*++

Routine Description:

    This routine gets a random value within the given range.

Arguments:

    Minimum - Supplies the minimum random value.

    Maximum - Supplies the maximum random value, exclusive.

Return Value:

    Returns a random value between the specified minimum and maximum.

--*/

{

    ssize_t BytesRead;
    INT Value;

    //
    // Prefer the random source if it exists.
    //

    if (DwRandomSource >= 0) {
        do {
            BytesRead = read(DwRandomSource, &Value, 2);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead == sizeof(Value)) {
            return Minimum + (Value % (Maximum - Minimum));
        }
    }

    return Minimum + (rand() % (Maximum - Minimum));
}

ULONG
DwChecksum (
    PVOID Buffer,
    ULONG Size
    )

/*++

Routine Description:

    This routine gets a checksum value for the high scores file.

Arguments:

    Buffer - Supplies a pointer to the buffer to checksum.

    Size - Supplies the size in bytes of the checksum.

Return Value:

    Returns the checksum of the file.

--*/

{

    PUCHAR Bytes;
    ULONG Sum;

    //
    // Borrow an algorithm from ELF: Sum = (Sum * 33) + Byte for each byte.
    //

    Bytes = Buffer;
    Sum = 0;
    while (Size != 0) {
        Sum = ((Sum << 5) + Sum) + *Bytes;
        Bytes += 1;
        Size -= 1;
    }

    return Sum;
}

