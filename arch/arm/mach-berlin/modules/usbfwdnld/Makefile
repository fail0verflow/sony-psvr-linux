# File: Makefile
#
# Copyright (C) 2014, Marvell International Ltd.
#
# This software file (the "File") is distributed by Marvell International
# Ltd. under the terms of the GNU General Public License Version 2, June 1991
# (the "License").  You may use, redistribute and/or modify this File in
# accordance with the terms and conditions of the License, a copy of which
# is available by writing to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
# worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
#
# A copy of the GPL is available in file gpl-2.0.txt accompanying in this
# deliverables.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
# this warranty disclaimer.

CC=		$(CROSS_COMPILE)gcc
LD=		$(CROSS_COMPILE)ld
BACKUP=		/root/backup
YMD=		`date +%Y%m%d%H%M`

#############################################################################
# Configuration Options
#############################################################################

# Debug Option
# DEBUG LEVEL n/1/2:
# n: NO DEBUG
# 1: Only PRINTM(MMSG,...), PRINTM(MFATAL,...), ...
# 2: All PRINTM()
CONFIG_DEBUG=1

# Big-endian platform
CONFIG_BIG_ENDIAN=n

#############################################################################
# Select Platform Tools
#############################################################################

MODEXT = ko
EXTRA_CFLAGS += -DLINUX

KERNELDIR ?= /usr/src/arm/linux-3.4.60-bg2
CROSS_COMPILE ?= /usr/local/arm-eabi-4.6/bin/arm-eabi-
INSTALLDIR ?= /tftpboot/pxa9xx/root
ARCH ?= arm


LD += -S
BINDIR = ../bin_usbfwdnld

#############################################################################
# Compiler Flags
#############################################################################

	EXTRA_CFLAGS += -I$(KERNELDIR)/include
ifeq ($(CONFIG_DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
endif

ifeq ($(CONFIG_BIG_ENDIAN),y)
	EXTRA_CFLAGS += -DBIG_ENDIAN_SUPPORT
endif

ifeq ($(CONFIG_64BIT), y)
	EXTRA_CFLAGS += -DDRIVER_64BIT
endif

#############################################################################
# Make Targets
#############################################################################


ifneq ($(KERNELRELEASE),)

USBOBJS = fw_dnld_usb.o

obj-m := usbfwdnld.o
usbfwdnld-objs := $(USBOBJS)



# Otherwise we were called directly from the command line; invoke the kernel build system.
else

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

endif

###############################################################

export		CC LD EXTRA_CFLAGS KERNELDIR

#	echo "Finished Making Marvell USB Fw Download Driver"

echo:

build:		echo default

	@if [ ! -d $(BINDIR) ]; then \
		mkdir $(BINDIR); \
	fi
	cp -f usbfwdnld.$(MODEXT) $(BINDIR)/
	cp -f README $(BINDIR)/


clean:
	-find . -name "*.o" -exec rm {} \;
	-find . -name "*.ko" -exec rm {} \;
	-find . -name ".*.cmd" -exec rm {} \;
	-find . -name "*.mod.c" -exec rm {} \;
	-find . -name "Module.symvers" -exec rm {} \;
	-find . -name "Module.markers" -exec rm {} \;
	-find . -name "modules.order" -exec rm {} \;
	-rm -rf .tmp_versions

distclean:
	-find . -name "*.o" -exec rm {} \;
	-find . -name "*.orig" -exec rm {} \;
	-find . -name "*.swp" -exec rm {} \;
	-find . -name "*.*~" -exec rm {} \;
	-find . -name "*~" -exec rm {} \;
	-find . -name "*.d" -exec rm {} \;
	-find . -name "*.a" -exec rm {} \;
	-find . -name "tags" -exec rm {} \;
	-find . -name ".*" -exec rm -rf 2> /dev/null \;
	-find . -name "*.ko" -exec rm {} \;
	-find . -name ".*.cmd" -exec rm {} \;
	-find . -name "*.mod.c" -exec rm {} \;
	-rm -rf .tmp_versions

# End of file
