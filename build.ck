/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    Minoca OS

Abstract:

    This module contains the top level build target for Minoca OS.

Author:

    Evan Green 14-Apr-2016

Environment:

    Build

--*/

function build() {
    cc = {
        "type": "tool",
        "name": "cc",
        "command": "$CC $CPPFLAGS $CFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    cxx = {
        "type": "tool",
        "name": "cxx",
        "command": "$CXX $CPPFLAGS $CFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    ld = {
        "type": "tool",
        "name": "ld",
        "command": "$CC $LDFLAGS -Wl,-Map=$OUT.map -o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    };

    ar = {
        "type": "tool",
        "name": "ar",
        "command": "$AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    };

    as = {
        "type": "tool",
        "name": "as",
        "command": "$CC $CPPFLAGS $ASFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    objcopy = {
        "type": "tool",
        "name": "objcopy",
        "command": "$SHELL -c \"cd `dirname $IN` && $OBJCOPY $OBJCOPY_FLAGS `basename $IN` $OUT\"",
        "description": "Objectifying - $IN"
    };

    build_cc = {
        "type": "tool",
        "name": "build_cc",
        "command": "$BUILD_CC $BUILD_CPPFLAGS $BUILD_CFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_cxx = {
        "type": "tool",
        "name": "build_cxx",
        "command": "$BUILD_CXX $BUILD_CPPFLAGS $BUILD_CFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_ld = {
        "type": "tool",
        "name": "build_ld",
        "command": "$BUILD_CC $BUILD_LDFLAGS -Wl,-Map=$OUT.map -o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    };

    build_ar = {
        "type": "tool",
        "name": "build_ar",
        "command": "$BUILD_AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    };

    build_as = {
        "type": "tool",
        "name": "build_as",
        "command": "$BUILD_CC $BUILD_CPPFLAGS $BUILD_ASFLAGS -MMD -MF $OUT.d -c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_objcopy = {
        "type": "tool",
        "name": "build_objcopy",
        "command": "$SHELL -c \"cd `dirname $IN` && $BUILD_OBJCOPY $BUILD_OBJCOPY_FLAGS `basename $IN` $OUT\"",
        "description": "Objectifying - $IN"
    };

    build_rcc = {
        "type": "tool",
        "name": "build_rcc",
        "command": "$RCC -o $OUT $IN",
        "description": "Compiling Resource - $IN",
    };

    iasl = {
        "type": "tool",
        "name": "iasl",
        "command": "$SHELL -c \"$IASL $IASL_FLAGS -p $OUT $IN > $OUT.stdout\"",
        "description": "Compiling ASL - $IN"
    };

    cp = {
        "type": "tool",
        "name": "copy",
        "command": "cp $CPFLAGS $IN $OUT",
        "description": "Copying - $IN -> $OUT"
    };

    stamp = {
        "type": "tool",
        "name": "stamp",
        "command": "$SHELL -c \"date > $OUT\"",
        "description": "Stamp - $OUT"
    };

    config_entry = {
        "type": "global_config",
        "config": global_config
    };

    pool1 = {
        "type": "pool",
        "name": "mypool1",
        "depth": 4
    };

    pool2 = {
        "type": "pool",
        "name": "mypool2",
        "depth": 1
    };

    entries = [cc, cxx, ld, ar, as, objcopy,
               build_cc, build_cxx, build_ld, build_ar, build_as, build_rcc,
               build_objcopy, iasl, cp, stamp,
               config_entry, pool1, pool2];

    all = [
        "//lib/crypto/testcryp:",
        "//apps/mingen:",
        "//lib/fatlib/fattest:",
        "//lib/rtl/testrtl:",
        "//lib/yy/yytest:",
        "//kernel/mm/testmm:",
        "//kernel:",
        "//apps/libc/crypt:",
        "//apps/libc/static:",
        "//apps/libc/dynamic:",
        "//apps/libc/dynamic/testc:",
        "//apps/libc/dynamic/pthread/static:",
        "//apps/efiboot:",
        "//apps/netcon:",
        "//apps/profile:",
        "//apps/setup:",
        "//apps/swiss:",
        "//apps/unmount:",
        "//apps/vmstat:",
        "//apps/testapps:testapps",
        "//boot/bootman:",
        "//boot/loader:",
        "//debug/client:",
        "//debug/client/testdisa:build_testdisa",
        "//debug/client/teststab:build_teststab",
        "//debug/client/tdwarf:build_tdwarf",
        "//debug/kexts:",
        "//drivers/acpi:acpi",
        "//drivers/ata:ata",
        "//drivers/devrem:devrem",
        "//drivers/fat:fat",
        "//drivers/i8042:i8042",
        "//drivers/net/ethernet/atl1c:atl1c",
        "//drivers/net/ethernet/dwceth:dwceth",
        "//drivers/net/ethernet/e100:e100",
        "//drivers/net/ethernet/rtl81xx:rtl81xx",
        "//drivers/net/ethernet/smsc91c1:smsc91c1",
        "//drivers/net/ethernet/smsc95xx:smsc95xx",
        "//drivers/net/wireless/rtlw81xx:rtlw81xx",
        "//drivers/null:null",
        "//drivers/part:part",
        "//drivers/pci:pci",
        "//drivers/plat/quark/qrkhostb:qrkhostb",
        "//drivers/ramdisk",
        "//drivers/sd/core:sd",
        "//drivers/special:special",
        "//drivers/term/ser16550:ser16550",
        "//drivers/videocon:videocon",
        "//drivers/usb/ehci:ehci",
        "//drivers/usb/onering:onering",
        "//drivers/usb/onering/usbrelay:usbrelay",
        "//drivers/usb/uhci:uhci",
        "//drivers/usb/usbcomp:usbcomp",
        "//drivers/usb/usbcore:usbcore",
        "//drivers/usb/usbhub:usbhub",
        "//drivers/usb/usbkbd:usbkbd",
        "//drivers/usb/usbmass:usbmass",
        "//tzcomp:tzdata",
        "//tzcomp:tzdflt"
    ];

    if (arch == "armv7") {
        all += [
            "//uefi/plat/beagbone:bbonefw",
            "//uefi/plat/beagbone/init:bbonemlo",
            "//uefi/plat/panda/init:omap4mlo",
            "//uefi/plat/integcp:integfw",
            "//uefi/plat/panda:pandafw",
            "//uefi/plat/panda:pandausb.img",
            "//uefi/plat/rpi2:rpi2fw",
            "//uefi/plat/veyron:veyronfw",
            "//drivers/dma/bcm2709:dmab2709",
            "//drivers/dma/edma3:edma3",
            "//drivers/gpio/rockchip/rk32:rk32gpio",
            "//drivers/gpio/ti/omap4:om4gpio",
            "//drivers/i8042/pl050:pl050",
            "//drivers/net/ethernet/am3eth:am3eth",
            "//drivers/plat/goec:goec",
            "//drivers/plat/rockchip/rk808:rk808",
            "//drivers/plat/ti/am3soc:am3soc",
            "//drivers/plat/ti/tps65217:tps65217",
            "//drivers/sd/bcm2709:sdbm2709",
            "//drivers/sd/omap4:sdomap4",
            "//drivers/sd/rk32xx:sdrk32xx",
            "//drivers/spb/i2c/am3i2c:am3i2c",
            "//drivers/spb/i2c/rk3i2c:rk3i2c",
            "//drivers/spb/spi/rk32spi:rk32spi",
            "//drivers/usb/am3usb:am3usb",
            "//drivers/usb/dwhci:dwhci",
        ];

    } else if (arch == "armv6") {
        all += [
            "//uefi/plat/rpi:rpifw"
        ];

    } else if (arch == "x86") {
        all += [
            "//uefi/plat/bios:biosfw",
            "//boot/fatboot:fatboot.bin",
            "//boot/mbr:mbr.bin"
        ];
    }

    entries += group("all", all);
    return entries;
}

return build();
