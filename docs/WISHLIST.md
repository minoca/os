# Wish List

If you're looking for a way to contribute to Minoca OS, but aren't sure where to start, this is the page for you. Below you'll find our current list of things we'd like to do but could use help with. We try to keep this list up to date. The General category lists abstract tasks that are always useful and can be anywhere on the map in terms of difficulty level. We've divided the more concrete tasks into three different difficulty levels. They're rough estimates and vague categories, as software is notoriously difficult to cost. Your mileage may vary. Within a category, tasks are in no particular order.

If you're serious about tackling one of the medium or hard tasks, feel free to shoot us an email first. We'll be able to provide a more detailed description, some context, and can offer suggestions towards getting started on any particular task.

Questions or patches can be submitted to minoca-dev@googlegroups.com. Good luck!

### General
 - Fix a bug
 - Port a new package over
 - Adopt a package: Clean up package diffs and work with maintainers to push upstream
 - Update a package to a newer version
 - Adopt a device: write a device driver
 - Port the OS to a new SoC or board
 - Write a test

### Easy
 - Fix a "TODO" in the code
 - Port a new package
 - Implement get{host,net,proto,srv}ent and friends.
 - AARCH64 disassembler
 - Implement hardware breakpoints in debugger/Kd
 - Floating point register access in debugger
 - Implement ResourceLimit*
 - Rework kernel device interface database
 - Serial (UART) device drivers

### Medium
 - Add TCP_CORK
 - Implement sigaltstack
 - Implement vfork
 - Implement statvfs
 - Implement pathconf
 - Add ATAPI to ATA driver
 - Isochronous USB transfers
 - Rework kernel support for user mode debugging/ptrace
 - Network bridge support
 - Virtual network devices
 - Namespaces
 - SD/MMC hardening (error recovery)
 - Crash dumps (application)
 - User mode profiling support
 - Mount improvements (flags, passing arguments to FS, C library integration)
 - Loopback block devices
 - UART flow control
 - Network/ramdisk boot
 - Add SD driver for PL081 used by Qemu (ARM).
 - NAND flash file system
 - IPv6 kernel support

### Hard
 - Implement better scheduling
 - Debugger 2.0
 - Port Clang
 - Port Go
 - Port Rust
 - Bluetooth stack
 - Audio stack
 - Crash Dumps (kernel)
 - Add support for better FS
 - AARCH64 architecture port
 - CD boot
 - Accelerated graphics

Below is a list of platforms that might be interesting to port to.
### Interesting Platform Ports
 - chipPC
 - Odroid C1/C2/XU4

### Community feature requests
 - PowerPC architecture port
 - MIPS architecture port
 - Asynchronous I/O mechanism
 - Port D package
