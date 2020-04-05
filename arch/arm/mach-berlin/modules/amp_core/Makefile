##############################################################################
 # Copyright (c) 2013-2014 Marvell International Ltd. and its affiliates.
 # All rights reserved.
 #
 # This software file (the "File") is owned and distributed by Marvell
 # International Ltd. and/or its affiliates ("Marvell") under the following
 # licensing terms.
 ##############################################################################
 # Marvell Commercial License Option
 #
 # If you received this File from Marvell and you have entered into a
 # commercial license agreement (a "Commercial License") with Marvell, the
 # File is licensed to you under the terms of the applicable Commercial
 # License.
 ##############################################################################
 # Marvell GPL License Option
 #
 # If you received this File from Marvell, you may opt to use, redistribute
 # and/or modify this File in accordance with the terms and conditions of the
 # General Public License Version 2, June 1991 (the "GPL License"), a copy of
 # which is available along with the File in the license.txt file or by writing
 # to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 # MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 # THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 # WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE
 # EXPRESSLY DISCLAIMED. The GPL License provides additional details about this
 # warranty disclaimer.
 ###############################################################################
########need to change this var to kernel src tree
ifeq ($(KERNELDIR),)
KERNEL_BUILD    := $(ANDROID_BUILD_TOP)/vendor/marvell-sdk/linux/
else
KERNEL_BUILD = $(KERNELDIR)
endif

ifneq ($(TARGET_DEVICE),)
BUILD_COMMON_CFG=$(ANDROID_BUILD_TOP)/vendor/marvell/generic/products/berlin.config
BUILD_PRODUCT_CFG=$(ANDROID_BUILD_TOP)/vendor/marvell/$(TARGET_DEVICE)/berlin.config

include $(BUILD_COMMON_CFG)
include $(BUILD_PRODUCT_CFG)
else
#NOTE. CURRENTLY USE BG2QA1 DMP BOARD FOR LINUXSDK. NEED CHANGE THIS WHEN CHANGE CHIPVERSION
MV_BERLIN_CHIP_VERSION ?= BERLIN_BG2Q_A1
endif


K_VERSION    = $(shell grep '^VERSION =' $(KERNEL_BUILD)/Makefile | cut -d ' ' -f 3)
K_PATCHLEVEL = $(shell grep '^PATCHLEVEL =' $(KERNEL_BUILD)/Makefile | cut -d ' ' -f 3)
K_SUBLEVEL   = $(shell grep '^SUBLEVEL =' $(KERNEL_BUILD)/Makefile | cut -d ' ' -f 3)

LIB_NAME = amp_core
ifeq ($(AMPSDK_RELEASED),yes)
LIB_OUT = $(MRVL_SDK_PREBUILT_DIR)/lib/modules
else
include $(BUILDSYS)/common.mak
LIB_OUT = $(SYS_INSTALL_DIR)/lib/modules
endif

AMPCORE_ROOT=`pwd`
MODDIR = $(AMPCORE_ROOT)/kernel/
MAKE_JOBS ?= 8

export CONFIG_AMPDRV_DEVICE=m
export ARCH=arm
LINUX_PLATFORM = Marvell_DMP_BG2CT
export PLATFORM=$(LINUX_PLATFORM)
ifeq ($(OS), Linux)
ifeq ($(strip $(CROSS_COMPILE)),)
export CROSS_COMPILE=arm-marvell-eabi-
endif
else
export CROSS_COMPILE=arm-eabi-
endif

ifeq ($(DEBUG),yes)
CPPFLAGS += -DFUSION_DEBUG_SKIRMISH_DEADLOCK
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CT)
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CD)
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
endif

ifeq ($(shell test -e $(KERNEL_BUILD)/include/linux/autoconf.h && echo yes),yes)
  AUTOCONF_H = -include $(KERNEL_BUILD)/include/linux/autoconf.h
endif

ifeq ($(shell test -e $(KERNEL_BUILD)/include/linux/config.h && echo yes),yes)
  CPPFLAGS += -DHAVE_LINUX_CONFIG_H
endif

check-version = $(shell expr \( $(K_VERSION) \* 65536 + $(K_PATCHLEVEL) \* 256 + $(K_SUBLEVEL) \) \>= \( $(1) \* 65536 + $(2) \* 256 + $(3) \))

.PHONY: all modules clean

all: modules

$(info MV_MISC_AVSETTINGS_TYPE=$(MV_MISC_AVSETTINGS_TYPE) )
modules:
	@echo ------------kernel module $(LIB_NAME) build begin------------
	@echo kernel is in $(KERNEL_BUILD) and version is $(K_SUBLEVEL)
	$(MAKE) -C $(KERNEL_BUILD) CC=$(CROSS_COMPILE)"gcc" \
		GS_INC_DIR=$(AMPCORE_ROOT)/inc \
		TEE_ROOT_DIR=$(AMPCORE_ROOT)/../../../../.. \
		SUBDIRS=$(MODDIR) modules -j$(MAKE_JOBS) \
		MV_BERLIN_CHIP_VERSION=$(MV_BERLIN_CHIP_VERSION)
	@mkdir -p $(LIB_OUT)
	@cp -rfu --preserve=timestamp $(MODDIR)*.ko $(LIB_OUT)
	@echo ------------kernel module $(LIB_NAME) build done------------ ${LIB_OUT}

clean:
	@find $(MODDIR) \( -name .git -prune \
		-o -name *.o -o -name *.d -o -name *.ko -o -name .*.o.cmd \
		-o -name amp_core.mod.* -o -name .amp_core.* \
		-o -name Module.symvers -o -name modules.order \) \
		-type f -print | xargs rm -f
	@find $(MODDIR) -name .tmp_versions -type d -print |  xargs rm -rf
