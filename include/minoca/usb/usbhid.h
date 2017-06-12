/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhid.h

Abstract:

    This header contains definitions for a USB HID parser.

Author:

    Evan Green 14-Mar-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef USBHID_API
#define USBHID_API __DLLIMPORT
#endif

//
// Define the maximum number of supported items in a report.
//

#define USB_HID_MAX_ITEMS 0x400

//
// Define the bitfields in the report item.
//

#define USB_HID_REPORT_ITEM_SIZE_0 0x00
#define USB_HID_REPORT_ITEM_SIZE_1 0x01
#define USB_HID_REPORT_ITEM_SIZE_2 0x02
#define USB_HID_REPORT_ITEM_SIZE_4 0x03
#define USB_HID_REPORT_ITEM_SIZE_MASK 0x03

#define USB_HID_REPORT_ITEM_MAIN 0x00
#define USB_HID_REPORT_ITEM_GLOBAL 0x04
#define USB_HID_REPORT_ITEM_LOCAL 0x08
#define USB_HID_REPORT_ITEM_TYPE_MASK 0x0C

#define USB_HID_REPORT_ITEM_TAG_MASK 0xF0

//
// Define main items.
//

#define USB_HID_ITEM_COLLECTION 0xA0
#define USB_HID_ITEM_END_COLLECTION 0xC0
#define USB_HID_ITEM_FEATURE 0xB0
#define USB_HID_ITEM_INPUT 0x80
#define USB_HID_ITEM_OUTPUT 0x90

//
// Define global items.
//

#define USB_HID_ITEM_USAGE_PAGE 0x04
#define USB_HID_ITEM_LOGICAL_MINIMUM 0x14
#define USB_HID_ITEM_LOGICAL_MAXIMUM 0x24
#define USB_HID_ITEM_PHYSICAL_MINIMUM 0x34
#define USB_HID_ITEM_PHYSICAL_MAXIMUM 0x44
#define USB_HID_ITEM_UNIT_EXPONENT 0x54
#define USB_HID_ITEM_UNIT 0x64
#define USB_HID_ITEM_REPORT_SIZE 0x74
#define USB_HID_ITEM_REPORT_ID 0x84
#define USB_HID_ITEM_REPORT_COUNT 0x94
#define USB_HID_ITEM_PUSH 0xA4
#define USB_HID_ITEM_POP 0xB4

//
// Define local items.
//

#define USB_HID_ITEM_USAGE 0x08
#define USB_HID_ITEM_USAGE_MINIMUM 0x18
#define USB_HID_ITEM_USAGE_MAXIMUM 0x28
#define USB_HID_ITEM_STRING 0x78

//
// Define the long item mask.
//

#define USB_HID_ITEM_LONG 0xFC

//
// Define the item mask, which combines both the type and the tag.
//

#define USB_HID_ITEM_MASK 0xFC

//
// Define the HID data item flags.
//

#define USB_HID_DATA_CONSTANT           (1 << 0)
#define USB_HID_DATA_DATA               (0 << 0)
#define USB_HID_DATA_VARIABLE           (1 << 1)
#define USB_HID_DATA_ARRAY              (0 << 1)
#define USB_HID_DATA_RELATIVE           (1 << 2)
#define USB_HID_DATA_ABSOLUTE           (0 << 2)
#define USB_HID_DATA_WRAP               (1 << 3)
#define USB_HID_DATA_NO_WRAP            (0 << 3)
#define USB_HID_DATA_NON_LINEAR         (1 << 4)
#define USB_HID_DATA_LINEAR             (0 << 4)
#define USB_HID_DATA_NO_PREFERRED_STATE (1 << 5)
#define USB_HID_DATA_PREFERRED_STATE    (0 << 5)
#define USB_HID_DATA_NULL_STATE         (1 << 6)
#define USB_HID_DATA_NO_NULL_STATE      (0 << 6)
#define USB_HID_DATA_VOLATILE           (1 << 7)
#define USB_HID_DATA_NON_VOLATILE       (0 << 7)
#define USB_HID_DATA_BUFFERED_BYTES     (1 << 8)
#define USB_HID_DATA_BITFIELD           (0 << 8)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USB_HID_USAGE_PAGE {
    HidPageUndefined = 0x0,
    HidPageGenericDesktop = 0x1,
    HidPageSimulation = 0x2,
    HidPageVr = 0x3,
    HidPageSport = 0x4,
    HidPageGame = 0x5,
    HidPageGenericDevice = 0x6,
    HidPageKeyboard = 0x7,
    HidPageLed = 0x8,
    HidPageButton = 0x9,
    HidPageOrdinal = 0xA,
    HidPageTelephony = 0xB,
    HidPageConsumer = 0xC,
    HidPageDigitizer = 0xD,
    HidPagePid = 0xF,
    HidPageUnicode = 0x10,
    HidPageAlphanumericDisplay = 0x14,
    HidPageMedicalInstruments = 0x40,
    HidPageMonitorPageMin = 0x80,
    HidPageMonitorPageMax = 0x83,
    HidPagePowerPageMin = 0x84,
    HidPagePowerPageMax = 0x87,
    HidPageBarCodeScanner = 0x8C,
    HidPageScale = 0x8D,
    HidPageMagneticStripe = 0x8E,
    HidPagePointOfSale = 0x8F,
    HidPageCamera = 0x90,
    HidPageArcade = 0x91,
    HidPageVendorDefinedMin = 0xFF00,
    HidPageVendorDefinedMax = 0xFFFF
} USB_HID_USAGE_PAGE, *PUSB_HID_USAGE_PAGE;

typedef enum _USB_HID_USAGE_DESKTOP {
    HidDesktopUndefined = 0x00,
    HidDesktopPointer = 0x01,
    HidDesktopMouse = 0x02,
    HidDesktopJoystick = 0x04,
    HidDesktopGamePad = 0x05,
    HidDesktopKeyboard = 0x06,
    HidDesktopKeypad = 0x07,
    HidDesktopMultiAxisController = 0x08,
    HidDesktopTablet = 0x09,
    HidDesktopX = 0x30,
    HidDesktopY = 0x31,
    HidDesktopZ = 0x32,
    HidDesktopRx = 0x33,
    HidDesktopRy = 0x34,
    HidDesktopRz = 0x35,
    HidDesktopSlider = 0x36,
    HidDesktopDial = 0x37,
    HidDesktopWheel = 0x38,
    HidDesktopHatSwitch = 0x39,
    HidDesktopCountedBuffer = 0x3A,
    HidDesktopByteCount = 0x3B,
    HidDesktopMotionWakeup = 0x3C,
    HidDesktopStart = 0x3D,
    HidDesktopSelect = 0x3E,
    HidDesktopVx = 0x40,
    HidDesktopVy = 0x41,
    HidDesktopVz = 0x42,
    HidDesktopVbrx = 0x43,
    HidDesktopVbry = 0x44,
    HidDesktopVbrz = 0x45,
    HidDesktopVno = 0x46,
    HidDesktopFeatureNotification = 0x47,
    HidDesktopResolutionMultiplier = 0x48,
    HidDesktopSystemControl = 0x80,
    HidDesktopSystemPowerDown = 0x81,
    HidDesktopSystemSleep = 0x82,
    HidDesktopSystemWakeUp = 0x83,
    HidDesktopSystemContextMenu = 0x84,
    HidDesktopSystemMainMenu = 0x85,
    HidDesktopSystemAppMenu = 0x86,
    HidDesktopSystemMenuHelp = 0x87,
    HidDesktopSystemMenuExit = 0x88,
    HidDesktopSystemMenuSelect = 0x89,
    HidDesktopSystemMenuRight = 0x8A,
    HidDesktopSystemMenuLeft = 0x8B,
    HidDesktopSystemMenuUp = 0x8C,
    HidDesktopSystemMenuDown = 0x8D,
    HidDesktopSystemColdRestart = 0x8E,
    HidDesktopSystemWarmRestart = 0x8F,
    HidDesktopDPadUp = 0x90,
    HidDesktopDPadDown = 0x91,
    HidDesktopDPadRight = 0x92,
    HidDesktopDPadLeft = 0x93,
    HidDesktopSystemDock = 0xA0,
    HidDesktopSystemUndock = 0xA1,
    HidDesktopSystemSetup = 0xA2,
    HidDesktopSystemBreak = 0xA3,
    HidDesktopSystemDebuggerBreak = 0xA4,
    HidDesktopApplicationBreak = 0xA5,
    HidDesktopApplicationDebuggerBreak = 0xA6,
    HidDesktopSystemSpeakerMute = 0xA7,
    HidDesktopSystemHibernate = 0xA8,
    HidDesktopSystemDisplayInvert = 0xB0,
    HidDesktopSystemDisplayInternal = 0xB1,
    HidDesktopSystemDisplayExternal = 0xB2,
    HidDesktopSystemDisplayBoth = 0xB3,
    HidDesktopSystemDisplayDual = 0xB4,
    HidDesktopSystemDisplayToggle = 0xB5,
    HidDesktopSystemDisplaySwap = 0xB6,
    HidDesktopSystemDisplayLcdAutoscale = 0xB7
} USB_HID_USAGE_DESKTOP, *PUSB_HID_USAGE_DESKTOP;

typedef enum _USB_HID_GENERIC_DEVICE {
    HidGenericDeviceUndefined = 0x00,
    HidGenericDeviceBatteryStrength = 0x20,
    HidGenericDeviceWirelessChannel = 0x21,
    HidGenericDeviceWirelessId = 0x22,
    HidGenericDeviceDiscoverWirelessControl = 0x23,
    HidGenericDeviceSecurityCodeCharacterEntered = 0x24,
    HidGenericDeviceSecurityCodeCharacterReleased = 0x25,
    HidGenericDeviceSecurityCodeCleared = 0x26,
} USB_HID_GENERIC_DEVICE, *PUSB_HID_GENERIC_DEVICE;

typedef enum _USB_HID_CONSUMER {
    HidConsumerUnassigned = 0x000,
    HidConsumerControl = 0x001,
    HidConsumerNumericKeypad = 0x002,
    HidConsumerProgrammableButtons = 0x003,
    HidConsumerMicrophone = 0x004,
    HidConsumerHeadphone = 0x005,
    HidConsumerGraphicEqualizer = 0x006,
    HidConsumerPlus10 = 0x020,
    HidConsumerPlus100 = 0x021,
    HidConsumerAmPm = 0x022,
    HidConsumerPower = 0x030,
    HidConsumerReset = 0x031,
    HidConsumerSleep = 0x032,
    HidConsumerSleepAfter = 0x033,
    HidConsumerSleepMode = 0x034,
    HidConsumerIllumination = 0x035,
    HidConsumerFunctionButtons = 0x036,
    HidConsumerMenu = 0x040,
    HidConsumerMenuPick = 0x041,
    HidConsumerMenuUp = 0x042,
    HidConsumerMenuDown = 0x043,
    HidConsumerMenuLeft = 0x044,
    HidConsumerMenuRight = 0x045,
    HidConsumerMenuEscape = 0x046,
    HidConsumerMenuValueIncrease = 0x047,
    HidConsumerMenuValueDecrease = 0x048,
    HidConsumerDataOnScreen = 0x060,
    HidConsumerClosedCaption = 0x061,
    HidConsumerClosedCaptionSelect = 0x062,
    HidConsumerVcrTv = 0x063,
    HidConsumerBroadcastMode = 0x064,
    HidConsumerSnapshot = 0x065,
    HidConsumerStill = 0x066,
    HidConsumerSelection = 0x080,
    HidConsumerAssignSelection = 0x081,
    HidConsumerModeStep = 0x082,
    HidConsumerRecallLast = 0x083,
    HidConsumerEnterChannel = 0x084,
    HidConsumerOrderMovie = 0x085,
    HidConsumerChannel = 0x086,
    HidConsumerMediaSelection = 0x087,
    HidConsumerMediaSelectComputer = 0x088,
    HidConsumerMediaSelectTv = 0x089,
    HidConsumerMediaSelectWww = 0x08A,
    HidConsumerMediaSelectDvd = 0x08B,
    HidConsumerMediaSelectTelephone = 0x08C,
    HidConsumerMediaSelectProgramGuide = 0x08D,
    HidConsumerMediaSelectVideoPhone = 0x08E,
    HidConsumerMediaSelectGames = 0x08F,
    HidConsumerMediaSelectMessages = 0x090,
    HidConsumerMediaSelectCd = 0x091,
    HidConsumerMediaSelectVcr = 0x092,
    HidConsumerMediaSelectTuner = 0x093,
    HidConsumerQuit = 0x094,
    HidConsumerHelp = 0x095,
    HidConsumerMediaSelectTape = 0x096,
    HidConsumerMediaSelectCable = 0x097,
    HidConsumerMediaSelectSatellite = 0x098,
    HidConsumerMediaSelectSecurity = 0x099,
    HidConsumerMediaSelectHome = 0x09A,
    HidConsumerMediaSelectCall = 0x09B,
    HidConsumerChannelIncrement = 0x09C,
    HidConsumerChannelDecrement = 0x09D,
    HidConsumerMediaSelectSap = 0x09E,
    HidConsumerVcrPlus = 0x0A0,
    HidConsumerOnce = 0x0A1,
    HidConsumerDaily = 0x0A2,
    HidConsumerWeekly = 0x0A3,
    HidConsumerMonthly = 0x0A4,
    HidConsumerPlay = 0x0B0,
    HidConsumerPause = 0x0B1,
    HidConsumerRecord = 0x0B2,
    HidConsumerFastForward = 0x0B3,
    HidConsumerRewind = 0x0B4,
    HidConsumerScanNextTrack = 0x0B5,
    HidConsumerScanPreviousTrack = 0x0B6,
    HidConsumerStop = 0x0B7,
    HidConsumerEject = 0x0B8,
    HidConsumerRandomPlay = 0x0B9,
    HidConsumerSelectDisc = 0x0BA,
    HidConsumerEnterDisk = 0x0BB,
    HidConsumerRepeat = 0x0BC,
    HidConsumerTracking = 0x0BD,
    HidConsumerTrackNormal = 0x0BE,
    HidConsumerSlowTracking = 0x0BF,
    HidConsumerFrameForward = 0x0C0,
    HidConsumerFrameBack = 0x0C1,
    HidConsumerMark = 0x0C2,
    HidConsumerClearMark = 0x0C3,
    HidConsumerRepeatFromMark = 0x0C4,
    HidConsumerReturnToMark = 0x0C5,
    HidConsumerSearchMarkForward = 0x0C6,
    HidConsumerSearchMarkBackwards = 0x0C7,
    HidConsumerCounterReset = 0x0C8,
    HidConsumerShowCounter = 0x0C9,
    HidConsumerTrackingIncrement = 0x0CA,
    HidConsumerTrackingDecrement = 0x0CB,
    HidConsumerStopEject = 0x0CC,
    HidConsumerPlayPause = 0x0CD,
    HidConsumerPlaySkip = 0x0CE,
    HidConsumerVolume = 0x0E0,
    HidConsumerBalance = 0x0E1,
    HidConsumerMute = 0x0E2,
    HidConsumerBass = 0x0E3,
    HidConsumerTreble = 0x0E4,
    HidConsumerBassBoost = 0x0E5,
    HidConsumerSurroundMode = 0x0E6,
    HidConsumerLoudness = 0x0E7,
    HidConsumerMpx = 0x0E8,
    HidConsumerVolumeIncrement = 0x0E9,
    HidConsumerVolumeDecrement = 0x0EA,
    HidConsumerSpeedSelect = 0x0F0,
    HidConsumerPlaybackSpeed = 0x0F1,
    HidConsumerStandardPlay = 0x0F2,
    HidConsumerLongPlay = 0x0F3,
    HidConsumerExtendedPlay = 0x0F4,
    HidConsumerSlow = 0x0F5,
    HidConsumerFanEnable = 0x100,
    HidConsumerFanSpeed = 0x101,
    HidConsumerLightEnable = 0x102,
    HidConsumerLightIlluminationLevel = 0x103,
    HidConsumerClimateControlEnable = 0x104,
    HidConsumerRoomTemperature = 0x105,
    HidConsumerSecurityEnable = 0x106,
    HidConsumerFireAlarm = 0x107,
    HidConsumerPoliceAlarm = 0x108,
    HidConsumerProximity = 0x109,
    HidConsumerMotion = 0x10A,
    HidConsumerDuressAlarm = 0x10B,
    HidConsumerHoldupAlarm = 0x10C,
    HidConsumerMedicalAlarm = 0x10D,
    HidConsumerBalanceRight = 0x150,
    HidConsumerBalanceLeft = 0x151,
    HidConsumerBassIncrement = 0x152,
    HidConsumerBassDecrement = 0x153,
    HidConsumerTrebleIncrement = 0x154,
    HidConsumerTrebleDecrement = 0x155,
    HidConsumerSpeakerSystem = 0x160,
    HidConsumerChannelLeft = 0x161,
    HidConsumerChannelRight = 0x162,
    HidConsumerChannelCenter = 0x163,
    HidConsumerChannelFront = 0x164,
    HidConsumerChannelCenterFront = 0x165,
    HidConsumerChannelSide = 0x166,
    HidConsumerChannelSurround = 0x167,
    HidConsumerChannelLowFrequencyEnhancement = 0x168,
    HidConsumerChannelTop = 0x169,
    HidConsumerChannelUnknown = 0x16A,
    HidConsumerSubchannel = 0x170,
    HidConsumerSubchannelIncrement = 0x171,
    HidConsumerSubchannelDecerment = 0x172,
    HidConsumerAlternateAudioIncrement = 0x173,
    HidConsumerAlternateAudioDecrement = 0x174,
    HidConsumerApplicationLaunchButtons = 0x180,
    HidConsumerAlLaunchButtonConfigurationTool = 0x181,
    HidConsumerAlProgrammableButtonConfiguration = 0x182,
    HidConsumerAlConsumerControlConfiguration = 0x183,
    HidConsumerAlWordProcessor = 0x184,
    HidConsumerAlTextEditor = 0x185,
    HidConsumerAlSpreadsheet = 0x186,
    HidConsumerAlGraphicsEditor = 0x187,
    HidConsumerAlPresentationApp = 0x188,
    HidConsumerAlDatabaseApp = 0x189,
    HidConsumerAlEmailReader = 0x18A,
    HidConsumerAlNewsreader = 0x18B,
    HidConsumerAlVoicemail = 0x18C,
    HidConsumerAlContacts = 0x18D,
    HidConsumerAlCalendar = 0x18E,
    HidConsumerAlTask = 0x18F,
    HidConsumerAlLog = 0x190,
    HidConsumerAlFinance = 0x191,
    HidConsumerAlCalculator = 0x192,
    HidConsumerAlAvCapturePlayback = 0x193,
    HidConsumerAlLocalMachineBrowser = 0x194,
    HidConsumerAlLanWanBrowser = 0x195,
    HidConsumerAlInternetBrowser = 0x196,
    HidConsumerAlRemoteNetworkingIspConnect = 0x197,
    HidConsumerAlNetworkConference = 0x198,
    HidConsumerAlNetworkChat = 0x199,
    HidConsumerAlTelephony = 0x19A,
    HidConsumerAlLogon = 0x19B,
    HidConsumerAlLogoff = 0x19C,
    HidConsumerAlLogonLogoff = 0x19D,
    HidConsumerAlLockScreensaver = 0x19E,
    HidConsumerAlControlPanel = 0x19F,
    HidConsumerAlCommandLineRun = 0x1A0,
    HidConsumerAlProcessManager = 0x1A1,
    HidConsumerAlSelectTask = 0x1A2,
    HidConsumerAlNextTask = 0x1A3,
    HidConsumerAlPreviousTask = 0x1A4,
    HidConsumerAlPreemptiveHaltTask = 0x1A5,
    HidConsumerAlIntegratedHelpCenter = 0x1A6,
    HidConsumerAlDocuments = 0x1A7,
    HidConsumerAlThesaurus = 0x1A8,
    HidConsumerAlDictionary = 0x1A9,
    HidConsumerAlDesktop = 0x1AA,
    HidConsumerAlSpellCheck = 0x1AB,
    HidConsumerAlGrammarCheck = 0x1AC,
    HidConsumerAlWirelessStatus = 0x1AD,
    HidConsumerAlKeyboardLayout = 0x1AE,
    HidConsumerAlVirusProtection = 0x1AF,
    HidConsumerAlEncryption = 0x1B0,
    HidConsumerAlScreensaver = 0x1B1,
    HidConsumerAlAlarms = 0x1B2,
    HidConsumerAlClock = 0x1B3,
    HidConsumerAlFileBrowser = 0x1B4,
    HidConsumerAlPowerStatus = 0x1B5,
    HidConsumerAlImageBrowser = 0x1B6,
    HidConsumerAlAudioBrowser = 0x1B7,
    HidConsumerAlMovieBrowser = 0x1B8,
    HidConsumerAlDigitalRightsManager = 0x1B9,
    HidConsumerAlDigitalWallet = 0x1BA,
    HidConsumerAlInstantMessaging = 0x1BC,
    HidConsumerAlOemFeatures = 0x1BCD,
    HidConsumerAlOemHelp = 0x1BE,
    HidConsumerAlOnlineCommunity = 0x1BF,
    HidConsumerAlEntertainmentContentBrowser = 0x1C0,
    HidConsumerAlOnlineShoppingBrowser = 0x1C1,
    HidConsumerAlSmartCardInformation = 0x1C2,
    HidConsumerAlMarketMonitor = 0x1C3,
    HidConsumerAlCustomizedCorporateNewsBrowser = 0x1C4,
    HidConsumerAlOnlineActivityBrowser = 0x1C5,
    HidConsumerAlSearchBrowser = 0x1C6,
    HidConsumerAlAudioPlayer = 0x1C7,
    HidConsumerGenericGuiApplicationControls = 0x200,
    HidConsumerAcNew = 0x201,
    HidConsumerAcOpen = 0x202,
    HidConsumerAcClose = 0x203,
    HidConsumerAcExit = 0x204,
    HidConsumerAcMaximize = 0x205,
    HidConsumerAcMinimize = 0x206,
    HidConsumerAcSave = 0x207,
    HidConsumerAcPrint = 0x208,
    HidConsumerAcProperties = 0x209,
    HidConsumerAcUndo = 0x21A,
    HidConsumerAcCopy = 0x21B,
    HidConsumerAcCut = 0x21C,
    HidConsumerAcPaste = 0x21D,
    HidConsumerAcSelectAll = 0x21E,
    HidConsumerAcFind = 0x21F,
    HidConsumerAcFindReplace = 0x220,
    HidConsumerAcSearch = 0x221,
    HidConsumerAcGoto = 0x222,
    HidConsumerAcHome = 0x223,
    HidConsumerAcBack = 0x224,
    HidConsumerAcForward = 0x225,
    HidConsumerAcStop = 0x226,
    HidConsumerAcRefresh = 0x227,
    HidConsumerAcPreviousLink = 0x228,
    HidConsumerAcNextLink = 0x229,
    HidConsumerAcBookmarks = 0x22A,
    HidConsumerAcHistory = 0x22B,
    HidConsumerAcSubscriptions = 0x22C,
    HidConsumerAcZoomIn = 0x22D,
    HidConsumerAcZoomOut = 0x22E,
    HidConsumerAcZoom = 0x22F,
    HidConsumerAcFullScreenView = 0x230,
    HidConsumerAcNormalView = 0x231,
    HidConsumerAcViewToggle = 0x232,
    HidConsumerAcScrollUp = 0x233,
    HidConsumerAcScrollDown = 0x234,
    HidConsumerAcScroll = 0x235,
    HidConsumerAcPanLeft = 0x236,
    HidConsumerAcPanRight = 0x237,
    HidConsumerAcPan = 0x238,
    HidConsumerAcNewWindow = 0x239,
    HidConsumerAcTileHorizontally = 0x23A,
    HidConsumerAcTileVertically = 0x23B,
    HidConsumerAcFormat = 0x23C,
    HidConsumerAcEdit = 0x23D,
    HidConsumerAcBold = 0x23E,
    HidConsumerAcItalics = 0x23F,
    HidConsumerAcUnderline = 0x240,
    HidConsumerAcStrikethrough = 0x241,
    HidConsumerAcSubscript = 0x242,
    HidConsumerAcSuperscript = 0x243,
    HidConsumerAcAllCaps = 0x244,
    HidConsumerAcRotate = 0x245,
    HidConsumerAcResize = 0x246,
    HidConsumerAcFlipHorizontal = 0x247,
    HidConsumerAcFlipVertical = 0x248,
    HidConsumerAcMirrorHorizontal = 0x249,
    HidConsumerAcMirrorVertical = 0x24A,
    HidConsumerAcFontSelect = 0x24B,
    HidConsumerAcFontColor = 0x24C,
    HidConsumerAcFontSize = 0x24D,
    HidConsumerAcJustifyLeft = 0x24E,
    HidConsumerAcJustifyCenterH = 0x24F,
    HidConsumerAcJustifyRight = 0x250,
    HidConsumerAcJustifyBlockH = 0x251,
    HidConsumerAcJustifyTop = 0x252,
    HidConsumerAcJustifyCenterV = 0x253,
    HidConsumerAcJustifyBottom = 0x254,
    HidConsumerAcJustifyBlockV = 0x255,
    HidConsumerAcIndentDecrease = 0x256,
    HidConsumerAcIndentIncrease = 0x257,
    HidConsumerAcNumberedList = 0x258,
    HidConsumerAcRestartNumbering = 0x259,
    HidConsumerAcBulletedList = 0x25A,
    HidConsumerAcPromote = 0x25B,
    HidConsumerAcDemote = 0x25C,
    HidConsumerAcYes = 0x25D,
    HidConsumerAcNo = 0x25E,
    HidConsumerAcCancel = 0x25F,
    HidConsumerAcCatalog = 0x260,
    HidConsumerAcCheckout = 0x261,
    HidConsumerAcAddToCart = 0x262,
    HidConsumerAcExpand = 0x263,
    HidConsumerAcExpandAll = 0x264,
    HidConsumerAcCollapse = 0x265,
    HidConsumerAcCollapseAll = 0x266,
    HidConsumerAcPrintPreview = 0x267,
    HidConsumerAcPasteSpecial = 0x268,
    HidConsumerAcInsertMode = 0x269,
    HidConsumerAcDelete = 0x26A,
    HidConsumerAcLock = 0x26B,
    HidConsumerAcUnlock = 0x26C,
    HidConsumerAcProtect = 0x26D,
    HidConsumerAcUnprotect = 0x26E,
    HidConsumerAcAttachComment = 0x26F,
    HidConsumerAcDeleteComment = 0x270,
    HidConsumerAcViewComment = 0x271,
    HidConsumerAcSelectWord = 0x272,
    HidConsumerAcSelectSentence = 0x273,
    HidConsumerAcSelectParagraph = 0x274,
    HidConsumerAcSelectColumn = 0x275,
    HidConsumerAcSelectRow = 0x276,
    HidConsumerAcSelectTable = 0x277,
    HidConsumerAcSelectObject = 0x278,
    HidConsumerAcRedo = 0x279,
    HidConsumerAcSort = 0x27A,
    HidConsumerAcSortAscending = 0x27B,
    HidConsumerAcSortDescending = 0x27C,
    HidConsumerAcFilter = 0x27D,
    HidConsumerAcSetClock = 0x27E,
    HidConsumerAcViewClock = 0x27F,
    HidConsumerAcSelectTimeZone = 0x280,
    HidConsumerAcEditTimeZones = 0x281,
    HidConsumerAcSetAlarm = 0x282,
    HidConsumerAcClearAlarm = 0x283,
    HidConsumerAcSnoozeAlarm = 0x284,
    HidConsumerAcResetAlarm = 0x285,
    HidConsumerAcSynchronize = 0x286,
    HidConsumerAcSendReceive = 0x287,
    HidConsumerAcSendTo = 0x288,
    HidConsumerAcReply = 0x289,
    HidConsumerAcReplyAll = 0x28A,
    HidConsumerAcForwardMessage = 0x28B,
    HidConsumerAcSend = 0x28C,
    HidConsumerAcAttachFile = 0x28D,
    HidConsumerAcUpload = 0x28E,
    HidConsumerAcDownload = 0x28F,
    HidConsumerAcSetBorders = 0x290,
    HidConsumerAcInsertRow = 0x291,
    HidConsumerAcInsertColumn = 0x292,
    HidConsumerAcInsertFile = 0x293,
    HidConsumerAcInsertPicture = 0x294,
    HidConsumerAcInsertObject = 0x295,
    HidConsumerAcInsertSymbol = 0x296,
    HidConsumerAcSaveAndClose = 0x297,
    HidConsumerAcRename = 0x298,
    HidConsumerAcMerge = 0x299,
    HidConsumerAcSplit = 0x29A,
    HidConsumerAcDistributeHorizontally = 0x29B,
    HidConsumerAcDistributeVertically = 0x29C
} USB_HID_CONSUMER, *PUSB_HID_CONSUMER;

typedef enum _USB_HID_DATA_TYPE {
    UsbhidDataInput,
    UsbhidDataOutput,
    UsbhidDataFeature,
    UsbhidDataTypeCount
} USB_HID_DATA_TYPE, *PUSB_HID_DATA_TYPE;

//
// Define the HID parser structure pointer.
//

typedef struct _USB_HID_PARSER USB_HID_PARSER, *PUSB_HID_PARSER;

typedef struct _USB_HID_COLLECTION_PATH
    USB_HID_COLLECTION_PATH, *PUSB_HID_COLLECTION_PATH;

/*++

Structure Description:

    This structure stores the tuple of usage page and value.

Members:

    Page - Stores the usage page.

    Usage - Stores the usage value.

--*/

typedef struct _USB_HID_USAGE {
    USHORT Page;
    USHORT Value;
} USB_HID_USAGE, *PUSB_HID_USAGE;

/*++

Structure Description:

    This structure stores the grouping of minimum and maximum values.

Members:

    Minimum - Stores the minimum value.

    Maximum - Stores the maximum value.

--*/

typedef struct _USB_HID_LIMITS {
    LONG Minimum;
    LONG Maximum;
} USB_HID_LIMITS, *PUSB_HID_LIMITS;

/*++

Structure Description:

    This structure stores the measurement unit information for a particular
    value.

Members:

    Type - Stores the unit type.

    Exponent - Stores the unit exponent.

--*/

typedef struct _USB_HID_UNIT {
    ULONG Type;
    UCHAR Exponent;
} USB_HID_UNIT, *PUSB_HID_UNIT;

/*++

Structure Description:

    This structure stores the scoping information for a collection.

Members:

    Type - Stores the collection path type (ie "Generic Desktop").

    Usage - Stores the usage information for this collection.

    Parent - Stores the parent of this collection if it is nested.

--*/

struct _USB_HID_COLLECTION_PATH {
    UCHAR Type;
    USB_HID_USAGE Usage;
    PUSB_HID_COLLECTION_PATH Parent;
};

/*++

Structure Description:

    This structure stores the set of properties describing an item.

Members:

    BitSize - Stores the size in bits of the field.

    Usage - Stores the usage page and value of the field.

    Unit - Stores the measurement unit of the field.

    LogicalLimit - Stores the logical limits of the field.

    PhysicalLimit - Stores the physical limits of the field.

--*/

typedef struct _USB_HID_ITEM_PROPERTIES {
    UCHAR BitSize;
    USB_HID_USAGE Usage;
    USB_HID_UNIT Unit;
    USB_HID_LIMITS LogicalLimit;
    USB_HID_LIMITS PhysicalLimit;
} USB_HID_ITEM_PROPERTIES, *PUSB_HID_ITEM_PROPERTIES;

/*++

Structure Description:

    This structure stores the information for one USB HID item.

Members:

    BitOffset - Stores the offset in bits from the start of the report to this
        item.

    ReportId - Stores the ID of this report.

    Type - Stores the type of data item this is.

    Properties - Stores the properties of this item.

    CollectionPath - Stores the collection path of this item.

    Flags - Stores a bitfield of item flags.

    Value - Stores the item value.

    PreviousValue - Stores the previous item value.

    SignBit - Stores the mask of which bit is the sign bit, for sign extensions.

--*/

typedef struct _USB_HID_ITEM {
    ULONG BitOffset;
    UCHAR ReportId;
    USB_HID_DATA_TYPE Type;
    USB_HID_ITEM_PROPERTIES Properties;
    PUSB_HID_COLLECTION_PATH CollectionPath;
    ULONG Flags;
    ULONG Value;
    ULONG PreviousValue;
    ULONG SignBit;
} USB_HID_ITEM, *PUSB_HID_ITEM;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

USBHID_API
PUSB_HID_PARSER
UsbhidCreateParser (
    VOID
    );

/*++

Routine Description:

    This routine creates a new USB HID parser.

Arguments:

    None.

Return Value:

    Returns a pointer to the new parser on success.

    NULL on allocation failure.

--*/

USBHID_API
VOID
UsbhidDestroyParser (
    PUSB_HID_PARSER Parser
    );

/*++

Routine Description:

    This routine destroys a HID parser.

Arguments:

    Parser - Supplies a pointer to the HID parser to destroy.

Return Value:

    None.

--*/

USBHID_API
KSTATUS
UsbhidParseReportDescriptor (
    PUSB_HID_PARSER Parser,
    PCUCHAR Data,
    UINTN Length
    );

/*++

Routine Description:

    This routine parses a HID report descriptor.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Data - Supplies a pointer to the descriptor bytes, just beyond the
        descriptor header.

    Length - Supplies the size of the descriptor array in bytes.

Return Value:

    Status code.

--*/

USBHID_API
VOID
UsbhidReadReport (
    PUSB_HID_PARSER Parser,
    PCUCHAR Report,
    UINTN Length
    );

/*++

Routine Description:

    This routine reads all fields of a report into their respective items.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Report - Supplies a pointer to the raw report.

    Length - Supplies the size of the report in bytes.

Return Value:

    None.

--*/

USBHID_API
VOID
UsbhidWriteReport (
    PUSB_HID_PARSER Parser,
    PUCHAR Report,
    UINTN Length
    );

/*++

Routine Description:

    This routine writes all fields of a report into a raw buffer.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Report - Supplies a pointer to the raw report.

    Length - Supplies the size of the report in bytes.

Return Value:

    None.

--*/

USBHID_API
KSTATUS
UsbhidReadItemData (
    PCUCHAR Report,
    ULONG ReportSize,
    PUSB_HID_ITEM Item
    );

/*++

Routine Description:

    This routine reads the contents from a report into the item value.

Arguments:

    Report - Supplies a pointer to the raw report.

    ReportSize - Supplies the size of the report in bytes.

    Item - Supplies a pointer to the item.

Return Value:

    STATUS_SUCCESS if the data was read.

    STATUS_DATA_LENGTH_MISMATCH if the report is not large enough for the item.

    STATUS_NO_MATCH if this report's ID does not correspond to this item.

--*/

USBHID_API
KSTATUS
UsbhidWriteItemData (
    PUSB_HID_ITEM Item,
    PUCHAR Report,
    ULONG ReportSize
    );

/*++

Routine Description:

    This routine writes the contents from an item into the raw report bytes.

Arguments:

    Item - Supplies a pointer to the item with the value to write.

    Report - Supplies a pointer to the raw report.

    ReportSize - Supplies the size of the report in bytes.

Return Value:

    STATUS_SUCCESS if the data was read.

    STATUS_DATA_LENGTH_MISMATCH if the report is not large enough for the item.

    STATUS_NO_MATCH if this report's ID does not correspond to this item.

--*/

USBHID_API
ULONG
UsbhidGetReportSize (
    PUSB_HID_PARSER Parser,
    UCHAR ReportId,
    USB_HID_DATA_TYPE Type
    );

/*++

Routine Description:

    This routine determines the length of the USB HID Report of the given type.

Arguments:

    Parser - Supplies a pointer to the parsed HID report.

    ReportId - Supplies the report ID. Supply 0 if the reports have no ID.

    Type - Supplies the type of report to get.

Return Value:

    Returns the number of bytes in the given type of report.

--*/

USBHID_API
PUSB_HID_ITEM
UsbhidFindItem (
    PUSB_HID_PARSER Parser,
    UCHAR ReportId,
    USB_HID_DATA_TYPE Type,
    PUSB_HID_USAGE Usage,
    PUSB_HID_ITEM StartFrom
    );

/*++

Routine Description:

    This routine determines the length of the USB HID Report of the given type.

Arguments:

    Parser - Supplies a pointer to the parsed HID report.

    ReportId - Supplies the report ID. Supply 0 if the reports have no ID, or
        to find an item from any ID.

    Type - Supplies the type of item to search for.

    Usage - Supplies the usage to search for.

    StartFrom - Supplies an item to start the search from, exclusive of the
        item itself. This can be used to find the next occurrence of the
        given usage. Supply NULL to start from the beginning.

Return Value:

    Returns a pointer to the found item on success.

    NULL if the item could not be found.

--*/

