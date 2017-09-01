################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       santa
#
#   Abstract:
#
#       This executable implements the Minoca package manager.
#
#   Author:
#
#       Evan Green 29-Aug-2017
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = santa

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

BINARY = santa.exe

endif

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = custom

OBJS = mkbundle.ck \
       santa.ck \
       cmd/archive.ck \
       cmd/build.ck \
       cmd/config.ck \
       cmd/convert_archive.ck \
       cmd/del_realm.ck \
       cmd/install.ck \
       cmd/list_realms.ck \
       cmd/new_realm.ck \
       cmd/patch.ck \
       cmd/uninstall.ck \
       containment/chroot.ck \
       containment/none.ck \
       presentation/copy.ck \
       presentation/move.ck \
       santa/build.ck \
       santa/config.ck \
       santa/containment.ck \
       santa/file.ck \
       santa/modules.ck \
       santa/presentation.ck \
       santa/lib/archive.ck \
       santa/lib/build.ck \
       santa/lib/config.ck \
       santa/lib/defaultbuild.ck \
       santa/lib/diff.ck \
       santa/lib/patch.ck \
       santa/lib/patchman.ck \
       santa/lib/pkg.ck \
       santa/lib/pkgbuilder.ck \
       santa/lib/pkgdb.ck \
       santa/lib/pkgdepot.ck \
       santa/lib/pkgman.ck \
       santa/lib/realm.ck \
       santa/lib/realmmanager.ck \
       santa/lib/santaconfig.ck

include $(SRCROOT)/os/minoca.mk

BUILD_COMMAND = chalk $(SRCDIR)/mkbundle.ck $@

