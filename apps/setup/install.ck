/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    install.ck

Abstract:

    This module defines the setup configuration data for installing Minoca
    OS.

Author:

    Evan Green 21-Oct-2016

Environment:

    Setup

--*/

from msetup import arch, plat;

//
// Constants
//

var KILOBYTE = 1024;
var MEGABYTE = KILOBYTE * 1024;
var GIGABYTE = MEGABYTE * 1024;
var TERABYTE = GIGABYTE * 1024;
var PETABYTE = TERABYTE * 1024;
var EXABYTE = PETABYTE * 1024;

var EFI_DEFAULT_APP;
if (arch == "x86") {
    EFI_DEFAULT_APP = "BOOTIA32.EFI";

} else if (arch == "x64") {
    EFI_DEFAULT_APP = "BOOTX64.EFI";

} else if ((arch == "armv7") || (arch == "armv6")) {
    EFI_DEFAULT_APP = "BOOTARM.EFI";
}

var PARTITION_FORMAT_NONE = 1;
var PARTITION_FORMAT_MBR = 2;
var PARTITION_FORMAT_GPT = 3;

var PARTITION_TYPE_EFI_SYSTEM =
    "\x28\x73\x2A\xC1\x1F\xF8\xD2\x11\xBA\x4B\x00\xA0\xC9\x3E\xC9\x3B";

var PARTITION_TYPE_MINOCA =
    "\xCC\x07\xA3\xCE\xBD\x78\x40\x6E\x81\x62\x60\x20\xAF\xB8\x8D\x17";

var PARTITION_ID_DOS_FAT12 = 0x01;
var PARTITION_ID_PRIMARY_FAT16 = 0x04;
var PARTITION_ID_FAT32 = 0x0B;
var PARTITION_ID_FAT32_LBA = 0x0C;
var PARTITION_ID_EFI_GPT = 0xEE;
var PARTITION_ID_MINOCA = 0x6B;

//
// Path configuration
//

//
// All of the source files are located inside of a directory rather than
// directly at the root because some file systems have a limited number of
// entries in the root directory (FAT12/16).
//

var SourceDir = "bin/";
var SystemRoot = "minoca/";
var SystemDirName = "system/";
var SystemDir = SystemRoot + SystemDirName;
var SystemConfigDir = SystemRoot + "config/";
var DriversDir = SystemRoot + "drivers/";
var EfiBootDir = "EFI/BOOT/";
var EfiMinocaDir = "EFI/MINOCA/";
var LoaderPathEfi = SystemDirName + "loadefi";
var LoaderPathPcat = SystemDirName + "loader";
var KernelPath = SystemDirName + "kernel";

//
// File lists
//

var DriverFiles = [
    "acpi.drv",
    "ehci.drv",
    "fat.drv",
    "net80211.drv",
    "netcore.drv",
    "null.drv",
    "onering.drv",
    "part.drv",
    "pci.drv",
    "rtlw81xx.drv",
    "rtlw8188eufw.bin",
    "rtlw8188cufwUMC.bin",
    "rtlw8192cufw.bin",
    "ser16550.drv",
    "sd.drv",
    "smsc95xx.drv",
    "sound.drv",
    "special.drv",
    "usbcomp.drv",
    "usbcore.drv",
    "usbhid.drv",
    "usbhub.drv",
    "usbkbd.drv",
    "usbmass.drv",
    "usbmouse.drv",
    "usrinput.drv",
    "videocon.drv",
];

if ((arch == "x86") || (arch == "x64")) {
    DriverFiles += [
        "ahci.drv",
        "ata.drv",
        "atl1c.drv",
        "dwceth.drv",
        "e100.drv",
        "e1000.drv",
        "i8042.drv",
        "intelhda.drv",
        "rtl81xx.drv",
        "uhci.drv",
        "pcnet32.drv",
    ];

} else if ((arch == "armv7") || (arch == "armv6")) {
    DriverFiles += [
        "dma.drv",
        "elani2c.drv",
        "gpio.drv",
        "spb.drv",
    ];
}

var BootDrivers = [
    "acpi.drv",
    "fat.drv",
    "null.drv",
    "part.drv",
    "special.drv",
    "videocon.drv",
];

if ((arch == "x86") || (arch == "x64")) {
    BootDrivers += [
        "ahci.drv",
        "ata.drv",
        "pci.drv",
        "ehci.drv",
        "usbcomp.drv",
        "usbhub.drv",
        "usbmass.drv",
        "sd.drv",
    ];
}

var SystemConfigFiles = [
    "dev2drv.set",
    "devmap.set",
    "init.set",
    "init.sh",
];

var SystemFiles = [
    "kernel",
    "loadefi",
    "bootmefi.efi",
    "libminocaos.so.1",
];

var SystemFilesX86Pcat = [
    "mbr.bin",
    "fatboot.bin",
    "bootman.bin",
    "loader",
];

//
// TODO: Remove this once the whole world compiles for x64.
//

if (arch == "x64") {
    SystemFiles = [
        "kernel",
        "libminocaos.so.1"
    ];
}

//
// Copy commands
//

var DriversCopy = {
    "Destination": DriversDir,
    "Source": SourceDir,
    "SourceVolume": 0,
    "Files": DriverFiles
};

var SystemCopy = {
    "Destination": SystemDir,
    "Source": SourceDir,
    "SourceVolume": 0,
    "Files": SystemFiles
};

var SystemConfigCopy = {
    "Destination": SystemConfigDir,
    "Source": SourceDir,
    "SourceVolume": 0,
    "Files": SystemConfigFiles
};

var BootmefiCopy = {
    "Destination": EfiBootDir,
    "Source": SourceDir + "bootmefi.efi",
    "SourceVolume": 0,
    "Update": true,
};

var BootmefiBackupCopy = {
    "Destination": EfiBootDir + "bootmefi.efi",
    "Source": SourceDir + "bootmefi.efi",
    "SourceVolume": 0,
};

var UserAppsCopy = {
    "Destination": "/apps/",
    "Source": SourceDir + "apps/",
    "SourceVolume": 0,
    "Optional": true
};

var UserSkelCopy = {
    "Destination": "/apps/",
    "Source": SourceDir + "skel/",
    "SourceVolume": 0,
};

var TotalBootCopy = [BootmefiCopy, BootmefiBackupCopy];
var TotalCopy = [
    DriversCopy,
    SystemCopy,
    SystemConfigCopy,
    UserSkelCopy,
    UserAppsCopy
];

//
// TODO: Remove this once the whole world compiles for x64.
//

if (arch == "x64") {
    TotalBootCopy = [];
}

//
// Partition descriptions
//

var BootPartition = {
    "Index": 0,
    "Size": 10 * MEGABYTE,
    "PartitionType": PARTITION_TYPE_EFI_SYSTEM,
    "MbrType": PARTITION_ID_PRIMARY_FAT16,
    "Files": TotalBootCopy,
    "Attributes": 0,
    "Alignment": 4 * KILOBYTE,
    "Flags": {
        "Boot": true,
        "System": false,
        "CompatibilityMode": true,
        "WriteVbrLba": false,
        "MergeVbr": false,
    },
};

var SystemPartition = {
    "Index": 1,
    "Size": -1,
    "PartitionType": PARTITION_TYPE_MINOCA,
    "MbrType": PARTITION_ID_MINOCA,
    "Files": TotalCopy,
    "Attributes": 0,
    "Alignment": 4 * KILOBYTE,
    "Flags": {
        "Boot": false,
        "System": true,
        "CompatibilityMode": false,
        "WriteVbrLba": false,
        "MergeVbr": false,
    },
};

var Partitions = [BootPartition, SystemPartition];

var DiskData = {
    "Format": PARTITION_FORMAT_GPT,
    "Partitions": Partitions
};

//
// Boot entry settings
//

var BootEntry = {
    "Name": "Minoca OS",
    "LoaderArguments": "",
    "KernelArguments": "",
    "LoaderPath": LoaderPathEfi,
    "KernelPath": KernelPath,
    "SystemPath": SystemRoot,
    "Flags": {
        "Debug": false,
        "BootDebug": false,
    },

    "DebugDevice": 0,
};

var BootConfiguration = {
    "Timeout": 0,
    "DataPath": EfiMinocaDir + "bootconf",
    "BootEntries": [BootEntry]
};

//
// Driver database configuration
//

var DriverDb = {
    "BootDrivers": BootDrivers,
    "BootDriversPath": SystemConfigDir + "bootdrv.set"
};

//
// Final settings compilation
//

var Settings = {
    "Disk": DiskData,
    "BootConfiguration": BootConfiguration,
    "DriverDb": DriverDb
};

//
// Individual platform configurations are defined below. Please try to keep
// these in alphabetical order.
//

//
// BeagleBone Black configuration
//

if (plat == "beagleboneblack") {
    DriversCopy["Files"] += [
        "am3eth.drv",
        "am3i2c.drv",
        "am3soc.drv",
        "am3usb.drv",
        "edma3.drv",
        "sdomap4.drv",
        "tps65217.drv",
    ];

    DriverDb["BootDrivers"] += [
        "am3soc.drv",
        "edma3.drv",
        "sdomap4.drv",
    ];

    var BboneFirmwareFiles = [
        "bbonefw",
        "bbonemlo",
    ];

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    var BboneFirmwareSystemCopy = {
        "Destination": SystemDir + "bbone/",
        "Source": SourceDir,
        "SourceVolume": 0,
        "Files": BboneFirmwareFiles
    };

    //
    // Copy the firmware to the boot partition.
    //

    var BboneFirmwareCopy = {
        "Destination": "/bbonefw",
        "Source": SourceDir + "bbonefw",
        "SourceVolume": 0,
    };

    TotalCopy.append(BboneFirmwareSystemCopy);
    TotalBootCopy.append(BboneFirmwareCopy);

    //
    // Set the MBR file.
    //

    var BboneMloCopy = {
        "Offset": 0,
        "Source": SourceDir + "bbonemlo",
        "SourceVolume": 0,
    };

    DiskData["Mbr"] = BboneMloCopy;
    DiskData["Format"] = PARTITION_FORMAT_MBR;
    BootPartition["Alignment"] = 1 * MEGABYTE;

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
}

//
// Galileo configuration
//

if (plat == "galileo") {
    DriversCopy["Files"] += [
        "qrkhostb.drv",
    ];

    DriverDb["BootDrivers"] += [
        "qrkhostb.drv",
    ];

    DiskData["Format"] = PARTITION_FORMAT_MBR;

    //
    // The Galileo uses the second debug device found for debugging.
    //

    BootEntry["DebugDevice"] = 1;
    BootEntry["KernelArguments"] += " ps.env=CONSOLE=/dev/Terminal/Slave2 ";

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
}

//
// ARMv6 install image
//

if (plat == "install-armv6") {

    //
    // List all the files in the bin directory that are ever installed
    // somewhere.
    //

    var Files = [
        "acpi.drv",
        "ahci.drv",
        "am3eth.drv",
        "am3i2c.drv",
        "am3soc.drv",
        "am3usb.drv",
        "ata.drv",
        "bc27gpio.drv",
        "bc27pwma.drv",
        "bootmefi.efi",
        "dev2drv.set",
        "devmap.set",
        "devrem.drv",
        "dma.drv",
        "dmab2709.drv",
        "dwhci.drv",
        "ehci.drv",
        "elani2c.drv",
        "fat.drv",
        "goec.drv",
        "gpio.drv",
        "init.set",
        "init.sh",
        "install.ck",
        "kernel",
        "kernel-version",
        "libc.so.1",
        "libcrypt.so.1",
        "libminocaos.so.1",
        "loadefi",
        "net80211.drv",
        "netcore.drv",
        "null.drv",
        "om4gpio.drv",
        "onering.drv",
        "part.drv",
        "pci.drv",
        "pl050.drv",
        "ramdisk.drv",
        "rk32gpio.drv",
        "rk32spi.drv",
        "rpifw",
        "rtlw81xx.drv",
        "rtlw8188eufw.bin",
        "rtlw8188cufwUMC.bin",
        "rtlw8192cufw.bin",
        "sd.drv",
        "sdbm2709.drv",
        "sdomap4.drv",
        "sdrk32xx.drv",
        "ser16550.drv",
        "smsc91c1.drv",
        "smsc95xx.drv",
        "sound.drv",
        "spb.drv",
        "special.drv",
        "tps65217.drv",
        "usbcomp.drv",
        "usbcore.drv",
        "usbhid.drv",
        "usbhub.drv",
        "usbkbd.drv",
        "usbmass.drv",
        "usbmouse.drv",
        "usrinput.drv",
        "videocon.drv",
    ];

    Files += [
        "skel/",
        "rpi/"
    ];

    var FilesCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": Files
    };

    var AppsCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": ["apps/"],
        "Optional": true
    };

    //
    // Create the storage partition.
    //

    var DataPartition = {
        "Index": 0,
        "Size": -1,
        "Files": [FilesCopy, AppsCopy],
    };

    DiskData["Partitions"] = [DataPartition];

    //
    // Perform no actual installation, just create an image that copies the files
    // into an image.
    //

    DiskData["Format"] = PARTITION_FORMAT_NONE;
    Settings = {
        "Disk": DiskData
    };
}

//
// ARMv7 install image
//

if (plat == "install-armv7") {

    //
    // List all the files in the bin directory that are ever installed
    // somewhere.
    //

    var Files = [
        "acpi.drv",
        "ahci.drv",
        "am3eth.drv",
        "am3i2c.drv",
        "am3soc.drv",
        "am3usb.drv",
        "ata.drv",
        "bbonefw",
        "bbonemlo",
        "bc27gpio.drv",
        "bc27pwma.drv",
        "bootmefi.efi",
        "dev2drv.set",
        "devmap.set",
        "devrem.drv",
        "dma.drv",
        "dmab2709.drv",
        "dwhci.drv",
        "edma3.drv",
        "ehci.drv",
        "elani2c.drv",
        "fat.drv",
        "goec.drv",
        "gpio.drv",
        "init.set",
        "init.sh",
        "install.ck",
        "kernel",
        "kernel-version",
        "libc.so.1",
        "libcrypt.so.1",
        "libminocaos.so.1",
        "loadefi",
        "net80211.drv",
        "netcore.drv",
        "null.drv",
        "om4gpio.drv",
        "omap4mlo",
        "onering.drv",
        "pandafw",
        "part.drv",
        "pci.drv",
        "pl050.drv",
        "ramdisk.drv",
        "rk32gpio.drv",
        "rk32spi.drv",
        "rk3i2c.drv",
        "rk808.drv",
        "rpi2fw",
        "rtlw81xx.drv",
        "rtlw8188eufw.bin",
        "rtlw8188cufwUMC.bin",
        "rtlw8192cufw.bin",
        "sd.drv",
        "sdbm2709.drv",
        "sdomap4.drv",
        "sdrk32xx.drv",
        "ser16550.drv",
        "smsc91c1.drv",
        "smsc95xx.drv",
        "sound.drv",
        "spb.drv",
        "special.drv",
        "tps65217.drv",
        "usbcomp.drv",
        "usbcore.drv",
        "usbhid.drv",
        "usbhub.drv",
        "usbkbd.drv",
        "usbmass.drv",
        "usbmouse.drv",
        "usrinput.drv",
        "veyronfw",
        "videocon.drv",
    ];

    Files += [
        "skel/",
        "rpi/"
    ];

    var FilesCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": Files
    };

    var AppsCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": ["apps/"],
        "Optional": true
    };

    //
    // Create the storage partition.
    //

    var DataPartition = {
        "Index": 0,
        "Size": -1,
        "Files": [FilesCopy, AppsCopy],
    };

    DiskData["Partitions"] = [DataPartition];

    //
    // Perform no actual installation, just create an image that copies the files
    // into an image.
    //

    DiskData["Format"] = PARTITION_FORMAT_NONE;
    Settings = {
        "Disk": DiskData
    };
}

//
// x86 install image
//

if ((plat == "install-x86") || (plat == "install-x64")) {

    //
    // List all the files in the bin directory that are ever installed
    // somewhere.
    //

    var Files = [
        "acpi.drv",
        "ahci.drv",
        "ata.drv",
        "atl1c.drv",
        "bootman.bin",
        "bootmefi.efi",
        "dev2drv.set",
        "devmap.set",
        "devrem.drv",
        "dwceth.drv",
        "e100.drv",
        "e1000.drv",
        "ehci.drv",
        "fat.drv",
        "fatboot.bin",
        "i8042.drv",
        "init.set",
        "init.sh",
        "install.ck",
        "intelhda.drv",
        "kernel",
        "kernel-version",
        "libc.so.1",
        "libcrypt.so.1",
        "libminocaos.so.1",
        "loader",
        "loadefi",
        "mbr.bin",
        "net80211.drv",
        "netcore.drv",
        "null.drv",
        "onering.drv",
        "part.drv",
        "pci.drv",
        "pcnet32.drv",
        "qrkhostb.drv",
        "rtl81xx.drv",
        "rtlw81xx.drv",
        "rtlw8188eufw.bin",
        "rtlw8188cufwUMC.bin",
        "rtlw8192cufw.bin",
        "sd.drv",
        "ser16550.drv",
        "smsc95xx.drv",
        "sound.drv",
        "special.drv",
        "uhci.drv",
        "usbcomp.drv",
        "usbcore.drv",
        "usbhid.drv",
        "usbhub.drv",
        "usbkbd.drv",
        "usbmass.drv",
        "usbmouse.drv",
        "usrinput.drv",
        "videocon.drv",
    ];

    //
    // TODO: Remove this once the whole world compiles for x64.
    //

    if (plat == "install-x64") {
        Files = [
            "acpi.drv",
            "ahci.drv",
            "ata.drv",
            "atl1c.drv",
            "bootman.bin",
            //"bootmefi.efi",
            "dev2drv.set",
            "devmap.set",
            "devrem.drv",
            "dwceth.drv",
            "e100.drv",
            "e1000.drv",
            "ehci.drv",
            "fat.drv",
            "fatboot.bin",
            "i8042.drv",
            "init.set",
            "init.sh",
            "install.ck",
            "intelhda.drv",
            "kernel",
            "kernel-version",
            "libc.so.1",
            "libcrypt.so.1",
            "libminocaos.so.1",
            "loader",
            //"loadefi",
            "mbr.bin",
            "net80211.drv",
            "netcore.drv",
            "null.drv",
            "onering.drv",
            "part.drv",
            "pci.drv",
            "pcnet32.drv",
            "qrkhostb.drv",
            "rtl81xx.drv",
            "rtlw81xx.drv",
            "rtlw8188eufw.bin",
            "rtlw8188cufwUMC.bin",
            "rtlw8192cufw.bin",
            "sd.drv",
            "ser16550.drv",
            "smsc95xx.drv",
            "sound.drv",
            "special.drv",
            "uhci.drv",
            "usbcomp.drv",
            "usbcore.drv",
            "usbhid.drv",
            "usbhub.drv",
            "usbkbd.drv",
            "usbmass.drv",
            "usbmouse.drv",
            "usrinput.drv",
            "videocon.drv",
        ];
    }

    Files += [
        "skel/"
    ];

    var FilesCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": Files
    };

    var AppsCopy = {
        "Destination": SourceDir,
        "Source": SourceDir,
        "SourceVolume": -1,
        "Files": ["apps/"],
        "Optional": true
    };

    //
    // Create the storage partition.
    //

    var DataPartition = {
        "Index": 0,
        "Size": -1,
        "Files": [FilesCopy, AppsCopy],
    };

    DiskData["Partitions"] = [DataPartition];

    //
    // Perform no actual installation, just create an image that copies the
    // files into an image.
    //

    DiskData["Format"] = PARTITION_FORMAT_NONE;
    Settings = {
        "Disk": DiskData
    };
}

//
// Integrator/CP RAM disk image
//

if (plat == "integrd") {
    DriversCopy["Files"] += [
        "ramdisk.drv",
        "pl050.drv",
        "smsc91c1.drv",
    ];

    DriverDb["BootDrivers"] += [
        "ramdisk.drv",
    ];

    //
    // Remove apps from the user files since it can be huge. Just add the bare
    // minimum.
    //

    UserAppsCopy["Source"] = "";

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
}

//
// PandaBoard image
//

if ((plat == "panda") || (plat == "panda-es")) {
    DriversCopy["Files"] += [
        "sdomap4.drv",
        "om4gpio.drv",
    ];

    DriverDb["BootDrivers"] += [
        "sdomap4.drv",
    ];

    var PandaFirmwareFiles = [
        "pandafw",
        "omap4mlo",
    ];

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    var PandaFirmwareSystemCopy = {
        "Destination": SystemDir + "panda/",
        "Source": SourceDir,
        "SourceVolume": 0,
        "Files": PandaFirmwareFiles
    };

    //
    // Copy the firmware to the boot partition.
    //

    var PandaFirmwareCopy = {
        "Destination": "/pandafw",
        "Source": SourceDir + "pandafw",
        "SourceVolume": 0,
    };

    TotalCopy.append(PandaFirmwareSystemCopy);
    TotalBootCopy.append(PandaFirmwareCopy);

    //
    // Set the MBR file.
    //

    var PandaMloCopy = {
        "Offset": 0,
        "Source": SourceDir + "omap4mlo",
        "SourceVolume": 0,
    };

    DiskData["Mbr"] = PandaMloCopy;
    DiskData["Format"] = PARTITION_FORMAT_MBR;
    BootPartition["Alignment"] = 1 * MEGABYTE;

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
}

//
// Pandaboard USB image
//

if (plat == "panda-usb") {
    DriversCopy["Files"] += [
        "ramdisk.drv",
        "om4gpio.drv",
        "sdomap4.drv",
        "smsc91c1.drv",
    ];

    DriverDb["BootDrivers"] += [
        "ramdisk.drv",
    ];

    //
    // Remove apps from the user files since it can be huge. Just add the bare
    // minimum.
    //

    UserAppsCopy["Source"] = "";

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
    BootPartition["Size"] = 3 * MEGABYTE;
    SystemPartition["Size"] = 20 * MEGABYTE;
}

//
// PC (BIOS) image
// TODO: Remove pc64 once x64 compiles enough to match x86.
//

if ((plat == "pc") || (plat == "pc64")) {

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    SystemCopy["Files"] += SystemFilesX86Pcat;

    //
    // Set the MBR file.
    //

    var MbrCopy = {
        "Offset": 0,
        "Source": SourceDir + "mbr.bin",
        "SourceVolume": 0,
    };

    DiskData["Mbr"] = MbrCopy;

    //
    // Set the VBR file.
    //

    var VbrCopy = {
        "Offset": 0,
        "Source": SourceDir + "fatboot.bin",
        "SourceVolume": 0,
    };

    BootPartition["Vbr"] = VbrCopy;
    BootPartition["Flags"]["MergeVbr"] = true;

    //
    // Replace the boot files with the boot manager.
    //

    var BootmanCopy = {
        "Destination": "/",
        "Source": SourceDir,
        "SourceVolume": 0,
        "Files": ["bootman.bin"]
    };

    BootPartition["Files"] = [BootmanCopy];
    BootPartition["Alignment"] = 1 * MEGABYTE;
    BootPartition["Flags"]["WriteVbrLba"] = true;
    BootEntry["LoaderPath"] = LoaderPathPcat;
    SystemPartition["Alignment"] = 1 * MEGABYTE;
    DiskData["Format"] = PARTITION_FORMAT_MBR;
}

//
// PC (UEFI) image
//

if (plat == "pcefi") {

    //
    // Add the BIOS files anyway for machines with a BIOS compatibility module.
    //

    SystemCopy["Files"] += SystemFilesX86Pcat;
    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
    BootPartition["Alignment"] = 1 * MEGABYTE;
    SystemPartition["Alignment"] = 1 * MEGABYTE;
}

//
// PC (tiny) image
//

if (plat == "pc-tiny") {

    //
    // Completely clobber the drivers list with one that fits x86 Qemu
    // perfectly.
    //

    DriversCopy["Files"] = [
        "acpi.drv",
        "fat.drv",
        "netcore.drv",
        "null.drv",
        "part.drv",
        "pci.drv",
        "special.drv",
        "usrinput.drv",
        "videocon.drv",
        "ata.drv",
        "e100.drv",
        "i8042.drv",
    ];

    DriverDb["BootDrivers"] = [
        "acpi.drv",
        "ata.drv",
        "fat.drv",
        "null.drv",
        "part.drv",
        "pci.drv",
        "special.drv",
        "videocon.drv",
    ];

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    SystemCopy["Files"] += SystemFilesX86Pcat;

    //
    // Remove apps from the user files since it can be huge. Just add the bare
    // minimum.
    //

    UserAppsCopy["Source"] = "";

    //
    // Set the MBR file.
    //

    var MbrCopy = {
        "Offset": 0,
        "Source": SourceDir + "mbr.bin",
        "SourceVolume": 0,
    };

    DiskData["Mbr"] = MbrCopy;

    //
    // Set the VBR file.
    //

    var VbrCopy = {
        "Offset": 0,
        "Source": SourceDir + "fatboot.bin",
        "SourceVolume": 0,
    };

    //
    // Replace the boot files with the boot manager.
    //

    var BootmanCopy = {
        "Destination": "/",
        "Source": SourceDir,
        "SourceVolume": 0,
        "Files": ["bootman.bin"]
    };

    var TotalCopies = [BootmanCopy] + SystemPartition["Files"];
    BootPartition = {
        "Index": 0,
        "Size": -1,
        "PartitionType": PARTITION_TYPE_MINOCA,
        "MbrType": PARTITION_ID_MINOCA,
        "Files": TotalCopies,
        "Attributes": 0,
        "Alignment": 4 * KILOBYTE,
        "Vbr": VbrCopy,
        "Flags": {
            "Boot": true,
            "System": true,
            "CompatibilityMode": false,
            "WriteVbrLba": true,
            "MergeVbr": true,
        },
    };

    DiskData["Partitions"] = [BootPartition];
    DiskData["Format"] = PARTITION_FORMAT_MBR;
    BootEntry["LoaderPath"] = LoaderPathPcat;
    BootPartition["Alignment"] = 1 * MEGABYTE;
    SystemPartition["Alignment"] = 1 * MEGABYTE;
}

//
// Raspberry Pi 1 image (ARMv6)
//

if (plat == "raspberrypi") {
    DriversCopy["Files"] += [
        "dwhci.drv",
        "dmab2709.drv",
        "sdbm2709.drv",
        "bc27gpio.drv",
        "bc27pwma.drv",
    ];

    DriverDb["BootDrivers"] += [
        "dmab2709.drv",
        "sdbm2709.drv",
    ];

    var RpiFirmwareBlobFiles = [
        "config.txt",
        "start.elf",
        "fixup.dat",
        "bootcode.bin",
        "LICENCE.broadcom"
    ];

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    var RpiFirmwareBlobsSystemCopy = {
        "Destination": SystemDir + "rpi/",
        "Source": SourceDir + "rpi/",
        "SourceVolume": 0,
        "Files": RpiFirmwareBlobFiles
    };

    var RpiFirmwareSystemCopy = {
        "Destination": SystemDir + "rpi/rpifw",
        "Source": SourceDir + "rpifw",
        "SourceVolume": 0
    };

    //
    // Copy the firmware to the boot partition.
    //

    var RpiFirmwareBlobsCopy = {
        "Destination": "/",
        "Source": SourceDir + "rpi/",
        "SourceVolume": 0,
        "Files": RpiFirmwareBlobFiles
    };

    var RpiFirmwareCopy = {
        "Destination": "/rpifw",
        "Source": SourceDir + "rpifw",
        "SourceVolume": 0,
    };

    TotalCopy.append(RpiFirmwareBlobsSystemCopy);
    TotalCopy.append(RpiFirmwareSystemCopy);
    TotalBootCopy.append(RpiFirmwareBlobsCopy);
    TotalBootCopy.append(RpiFirmwareCopy);
    DiskData["Format"] = PARTITION_FORMAT_MBR;

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
    BootPartition["Alignment"] = 1 * MEGABYTE;
    BootPartition["Size"] = 20 * MEGABYTE;
}

//
// Raspberry Pi 2 and 3 (32-bit only).
//

if (plat == "raspberrypi2") {
    DriversCopy["Files"] += [
        "dwhci.drv",
        "dmab2709.drv",
        "sdbm2709.drv",
        "bc27gpio.drv",
        "bc27pwma.drv",
    ];

    DriverDb["BootDrivers"] += [
        "dmab2709.drv",
        "sdbm2709.drv",
    ];

    var RpiFirmwareFiles = [
        "config.txt",
        "start.elf",
        "fixup.dat",
        "bootcode.bin",
        "LICENCE.broadcom"
    ];

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    var RpiFirmwareSystemCopy = {
        "Destination": SystemDir + "rpi/",
        "Source": SourceDir + "rpi/",
        "SourceVolume": 0,
        "Files": RpiFirmwareFiles
    };

    var Rpi2FirmwareSystemCopy = {
        "Destination": SystemDir + "rpi2/rpifw",
        "Source": SourceDir + "rpi2fw",
        "SourceVolume": 0,
    };

    //
    // Copy the firmware to the boot partition.
    //

    var RpiFirmwareCopy = {
        "Destination": "/",
        "Source": SourceDir + "rpi/",
        "SourceVolume": 0,
        "Files": RpiFirmwareFiles
    };

    //
    // The config.txt is expecting the name rpifw, so do a special copy and rename
    // for Raspberry Pi 2.
    //

    var Rpi2FirmwareCopy = {
        "Destination": "/rpifw",
        "Source": SourceDir + "rpi2fw",
        "SourceVolume": 0
    };

    TotalCopy.append(RpiFirmwareSystemCopy);
    TotalCopy.append(Rpi2FirmwareSystemCopy);
    TotalBootCopy.append(RpiFirmwareCopy);
    TotalBootCopy.append(Rpi2FirmwareCopy);
    DiskData["Format"] = PARTITION_FORMAT_MBR;

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
    BootPartition["Alignment"] = 1 * MEGABYTE;
    BootPartition["Size"] = 20 * MEGABYTE;
}

if (plat == "veyron") {
    DriversCopy["Files"] += [
        "dwhci.drv",
        "sdrk32xx.drv",
        "rk32spi.drv",
        "goec.drv",
        "rk32gpio.drv",
        "rk3i2c.drv",
        "rk808.drv"
    ];

    //
    // I2c and the RK808 PMIC are needed to bring up SD, since SD makes voltage
    // changes via the PMIC.
    //

    DriverDb["BootDrivers"] += [
        "sdrk32xx.drv",
        "rk32gpio.drv",
        "rk3i2c.drv",
        "rk808.drv"
    ];

    var VeyronFirmwareCopy = {
        "Offset": 0,
        "Source": SourceDir + "veyronfw",
        "SourceVolume": 0
    };

    //
    // Set up the correct partition layout for verified boot, which has a
    // firmware partition at the beginning with the raw firmware image in it.
    //

    var PARTITION_TYPE_VBOOT_FIRMWARE =
        "\x5D\x2A\x3A\xFE\x32\x4F\xA7\x41\xb7\x25\xAC\xCC\x32\x85\xA3\x09";

    var VeyronFirmwarePartition = {
        "Index": 0,
        "Size": 1 * MEGABYTE,
        "PartitionType": PARTITION_TYPE_VBOOT_FIRMWARE,
        "Files": [],
        "Attributes": 0x01FF000000000000,
        "Vbr": VeyronFirmwareCopy,
        "Flags": {
            "Boot": false,
            "System": false
        },
    };

    BootPartition["Index"] += 1;
    SystemPartition["Index"] += 1;
    Partitions.append(VeyronFirmwarePartition);

    //
    // Copy the firmware to the system partition for recovery if needed.
    //

    var VeyronFirmwareFiles = [
        "veyronfw",
    ];

    var VeyronFirmwareSystemCopy = {
        "Destination": SystemDir + "veyron/",
        "Source": SourceDir,
        "SourceVolume": 0,
        "Files": VeyronFirmwareFiles
    };

    TotalCopy.append(VeyronFirmwareSystemCopy);

    //
    // Set the EFI boot manager name.
    //

    BootmefiCopy["Destination"] += EFI_DEFAULT_APP;
}

