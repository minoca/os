################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       posix
#
#   Abstract:
#
#       This directory contains the template files for a POSIX-like environment.
#
#   Author:
#
#       Evan Green 19-Mar-2015
#
#   Environment:
#
#       User Mode
#
################################################################################

include $(SRCROOT)/os/minoca.mk

BINFILES=$(BINROOT)/skel/usr/sbin/update-rc.d       \
         $(BINROOT)/skel/var/run/utmp               \
         $(BINROOT)/skel/var/log/wtmp               \
         $(BINROOT)/skel/etc/passwd                 \
         $(BINROOT)/skel/etc/group                  \
         $(BINROOT)/skel/etc/shadow                 \
         $(BINROOT)/skel/etc/inittab                \
         $(BINROOT)/skel/etc/issue                  \
         $(BINROOT)/skel/etc/init.d/rc              \
         $(BINROOT)/skel/etc/init.d/init-functions  \
         $(BINROOT)/skel/etc/init.d/hostname.sh     \

BINDIRS=$(BINROOT)/skel/etc/init.d      \
        $(BINROOT)/skel/var/run         \
        $(BINROOT)/skel/var/log         \
        $(BINROOT)/skel/root            \
        $(BINROOT)/skel/home            \
        $(BINROOT)/skel/usr/sbin        \
        $(BINROOT)/skel/tmp             \

all: $(BINFILES)

$(BINFILES): | $(BINDIRS)

$(BINDIRS):
	mkdir -p $@

$(BINROOT)/skel/usr/sbin/update-rc.d: update-rc.d
	cp $^ $@
	chmod 0755 $@

$(BINROOT)/skel/etc/passwd: passwd
	cp $^ $@
	chmod 0644 $@

$(BINROOT)/skel/etc/group: group
	cp $^ $@
	chmod 0644 $@

$(BINROOT)/skel/etc/shadow: shadow
	cp $^ $@
	chmod 0640 $@

$(BINROOT)/skel/etc/inittab: inittab
	cp $^ $@
	chmod 0644 $@

$(BINROOT)/skel/etc/issue: issue
	cp $^ $@
	chmod 0644 $@

$(BINROOT)/skel/etc/init.d/rc: init.d/rc
	cp $^ $@
	chmod 0755 $@

$(BINROOT)/skel/etc/init.d/init-functions: init.d/init-functions
	cp $^ $@
	chmod 0755 $@

$(BINROOT)/skel/etc/init.d/hostname.sh: init.d/hostname.sh $(BINROOT)/skel/usr/sbin/update-rc.d
	cp $< $@
	chmod 0755 $@
	SYSROOT=$(BINROOT)/skel sh $(BINROOT)/skel/usr/sbin/update-rc.d -f hostname.sh defaults 10

$(BINROOT)/skel/var/run/utmp $(BINROOT)/skel/var/log/wtmp:
	touch $@

