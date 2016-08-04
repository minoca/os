################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       C Library
#
#   Abstract:
#
#       This module contains the dynamic library portion of the C library,
#       which is nearly everything. Implementing the bulk of the functionality
#       in a dynamic library reduces wasted space on disk and in memory across
#       processes by reducing duplicated code. It also allows the C library to
#       be updated without recompiling all applications.
#
#   Author:
#
#       Evan Green 4-Mar-2013
#
#   Environment:
#
#       User Mode C Library
#
################################################################################

BINARY = libc.so.1

BINARYTYPE = so

BINPLACE = bin

##
## Link in all object files of the included libraries as they are exported.
##

EXTRA_LDFLAGS += -Wl,--whole-archive

##
## Use the initial-exec TLS model, as it's faster for all those C library
## functions that need to set errno. This means the C library can only be
## loaded at startup, not dynamically loaded later.
##

EXTRA_CFLAGS += -ftls-model=initial-exec

ENTRY = ClInitialize

INCLUDES += $(SRCROOT)/os/apps/include;$(SRCROOT)/os/apps/include/libc;

OBJS = assert.o             \
       brk.o                \
       bsearch.o            \
       convert.o            \
       ctype.o              \
       dirio.o              \
       dynlib.o             \
       env.o                \
       err.o                \
       errno.o              \
       exec.o               \
       exit.o               \
       fileio.o             \
       fnmatch.o            \
       gaddrinf.o           \
       getopt.o             \
       glob.o               \
       heap.o               \
       if.o                 \
       inet.o               \
       init.o               \
       kerror.o             \
       langinfo.o           \
       line.o               \
       locale.o             \
       math/abs.o           \
       math/ceil.o          \
       math/div.o           \
       math/exp.o           \
       math/floor.o         \
       math/fmod.o          \
       math/hypot.o         \
       math/log.o           \
       math/log2.o          \
       math/log10.o         \
       math/lround.o        \
       math/modf.o          \
       math/pow.o           \
       math/rint.o          \
       math/scalbn.o        \
       math/sqrt.o          \
       math/trig.o          \
       math/trigarc.o       \
       math/trighyp.o       \
       math/util.o          \
       memory.o             \
       netaddr.o            \
       netent.o             \
       passwd.o             \
       path.o               \
       pid.o                \
       pthread/atfork.o     \
       pthread/barrier.o    \
       pthread/cond.o       \
       pthread/key.o        \
       pthread/mutex.o      \
       pthread/once.o       \
       pthread/pthread.o    \
       pthread/rwlock.o     \
       pthread/sema.o       \
       pthread/setids.o     \
       pthread/thrattr.o    \
       pty.o                \
       qsort.o              \
       rand.o               \
       random.o             \
       realpath.o           \
       regexcmp.o           \
       regexexe.o           \
       resolv.o             \
       resource.o           \
       setjmp.o             \
       scan.o               \
       sched.o              \
       shadow.o             \
       signals.o            \
       socket.o             \
       spawn.o              \
       stat.o               \
       statvfs.o            \
       stream.o             \
       streamex.o           \
       string.o             \
       sysconf.o            \
       syslog.o             \
       system.o             \
       termios.o            \
       time.o               \
       times.o              \
       tmpfile.o            \
       uio.o                \
       uname.o              \
       usershel.o           \
       utmpx.o              \
       wchar.o              \
       wctype.o             \
       wscan.o              \
       wstream.o            \
       wstring.o            \

ARMV7_OBJS = armv7/fenva.o    \
             armv7/fenvc.o    \
             armv7/setjmpa.o  \
             armv7/tlsaddr.o  \

ARMV6_OBJS = $(ARMV7_OBJS)

X86_OBJS = x86/fenv.o    \
           x86/fenvc.o   \
           x86/setjmpa.o \
           x86/tlsaddr.o \

X64_OBJS = x64/fenv.o    \
           x64/setjmpa.o \
           x86/fenvc.o   \

EXTRA_SRC_DIRS = x86 x64 armv7 math pthread

DYNLIBS = $(BINROOT)/libminocaos.so.1           \

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/intrins/intrins.a        \

DIRS = wincsup pthread

TESTDIRS = testc

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/lib/$(BINARY); then \
	    cp $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib/ ; \
	fi

