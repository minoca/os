################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Hardware Layer
#
#   Abstract:
#
#       This library contains the Hardware Layer, which provides abstractions
#       for core system resources such as timers, interrupt controllers,
#       cache controllers, and serial ports. These are devices that are
#       too central to the functioning of the system to be abstracted to
#       device drivers, as they are required for normal operation of the
#       kernel.
#
#   Author:
#
#       Evan Green 5-Aug-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = hl.a

BINARYTYPE = library

BINPLACE = intlib

OBJS = cache.o    \
       calendar.o \
       clock.o    \
       dbgdev.o   \
       efi.o      \
       hmodapi.o  \
       info.o     \
       init.o     \
       intlevel.o \
       intrupt.o  \
       ipi.o      \
       profiler.o \
       reset.o    \
       timer.o    \

ARMV7_OBJS = armv7/am335int.o \
             armv7/am335pwr.o \
             armv7/am335tmr.o \
             armv7/apinit.o   \
             armv7/apstart.o  \
             armv7/archcach.o \
             armv7/archdbg.o  \
             armv7/archintr.o \
             armv7/archrst.o  \
             armv7/archtimr.o \
             armv7/cpintr.o   \
             armv7/cptimer.o  \
             armv7/cyccnt.o   \
             armv7/cycsupc.o  \
             armv7/gic.o      \
             armv7/gicid.o    \
             armv7/omapintr.o \
             armv7/omap3pwr.o \
             armv7/omap3tmr.o \
             armv7/omap4chc.o \
             armv7/omap4pwr.o \
             armv7/omap4smc.o \
             armv7/omap4tmr.o \
             armv7/regacces.o \
             armv7/rk32tmr.o  \
             armv7/sp804tmr.o \
             armv7/uartpl11.o \
             armv7/uartomap.o \
             armv7/b2709id.o  \
             armv7/b2709int.o \
             armv7/b2709tmr.o \
             ns16550.o        \

ARMV6_OBJS = armv6/apstart.o  \
             armv6/archcach.o \
             armv6/archdbg.o  \
             armv6/archintr.o \
             armv6/archtimr.o \
             armv6/cycsupc.o  \
             armv7/archrst.o  \
             armv7/apinit.o   \
             armv7/b2709int.o \
             armv7/b2709tmr.o \
             armv7/cyccnt.o   \
             armv7/regacces.o \
             armv7/uartpl11.o \

X86_OBJS = ns16550.o      \
           x86/apic.o     \
           x86/apictimr.o \
           x86/apinit.o   \
           x86/apstart.o  \
           x86/archcach.o \
           x86/archdbg.o  \
           x86/archintr.o \
           x86/archsup.o  \
           x86/archrst.o  \
           x86/archtimr.o \
           x86/ioport.o   \
           x86/pmtimer.o  \
           x86/regacces.o \
           x86/rtc.o      \
           x86/tsc.o      \

EXTRA_SRC_DIRS = x86 armv7 armv6

DIRS = boot

boot: $(BINARY)

include $(SRCROOT)/os/minoca.mk

