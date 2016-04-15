/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Boot Manager

Abstract:

    This module implements the boot manager, which can load an operating
    system loader. In a multi-boot system, there would be one boot manager
    that can load one of many OS loaders (including downstream loaders).

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

function build() {
    common_sources = [
        "bootman.c",
        "bootim.c"
    ];

    pcat_sources = [
        "pcat/x86/entry.S",
        ":bootman.o",
        ":bootim.o",
        "pcat/bootxfr.c",
        "pcat/main.c",
    ];

    efi_sources = [
        "efi/bootxfr.c",
        "efi/main.c"
    ];

    bm_cppflags = [
        "-I$///boot/lib/include",
        "-I$///boot/bootman"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + bm_cppflags
    };

    efi_link_ldflags = [
        "-nostdlib",
        "-pie",
        "-static",
    ];

    pcat_link_ldflags = [
        "-nostdlib",
        "-static"
    ];

    efi_libs = [
        "//boot/lib:bootefi",
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        linker_script = "$///uefi/include/link_arm.x";
        efi_link_ldflags += [
            "-Wl,--no-wchar-size-warning"
        ];

        efi_libs = ["//kernel:archboot"] + efi_libs;

    } else if (arch == "x86") {
        linker_script = "$///uefi/include/link_x86.x";
    }

    efi_link_config = {
        "LDFLAGS": ["$LDFLAGS"] + efi_link_ldflags
    };

    pcat_link_config = {
        "LDFLAGS": ["$LDFLAGS"] + pcat_link_ldflags
    };

    //
    // These base libraries are relied upon by the boot library and so they
    // must go after the boot library.
    //

    base_libs = [
        "//lib/basevid:basevid",
        "//lib/fatlib:fat",
        "//kernel/mm:mmboot",
        "//lib/rtl/kmode:krtl",
        "//lib/rtl/base:basertlb"
    ];

    common_libs = [
        "//kernel/kd:kdboot",
        "//kernel/hl:hlboot",
        "//lib/im:im",
        "//lib/bconflib:bconf",
        "//kernel/kd/kdusb:kdnousb"
    ];

    pcat_libs = [
        "//boot/lib:bootpcat",
        "//lib/partlib:partlib"
    ];

    efi_app_libs = common_libs + efi_libs + base_libs;
    efi_app = {
        "label": "bootmefi.elf",
        "inputs": common_sources + efi_sources + efi_app_libs,
        "sources_config": sources_config,
        "config": efi_link_config,
        "entry": "BmEfiApplicationMain",
        "linker_script": linker_script
    };

    entries = application(efi_app);

    //
    // Convert the ELF image into an EFI PE image.
    //

    elfconv_config = {
        "ELFCONV_FLAGS": "-t efiapp"
    };

    bootman_pe = {
        "type": "target",
        "label": "bootmefi.efi",
        "inputs": [":bootmefi.elf"],
        "implicit": ["//uefi/tools/elfconv:elfconv"],
        "tool": "elfconv",
        "config": elfconv_config
    };

    entries += [bootman_pe];

    //
    // On PC machines, build the BIOS library as well.
    //

    if (arch == "x86") {
        pcat_app_libs = common_libs + pcat_libs + base_libs;
        pcat_app = {
            "label": "bootman.elf",
            "inputs": pcat_sources + pcat_app_libs,
            "sources_config": sources_config,
            "config": pcat_link_config,
            "text_address": "0x100000",
        };

        entries += executable(pcat_app);

        //
        // Flatten the image so the VBR can load it directly into memory.
        //

        flattened = {
            "label": "bootman.bin",
            "inputs": [":bootman.elf"]
        };

        flattened = flattened_binary(flattened);
        entries += flattened;
    }

    return entries;
}

return build();
