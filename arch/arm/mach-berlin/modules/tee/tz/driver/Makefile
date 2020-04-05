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
obj-m := tzd.o
tzd-y = tz_driver.o tz_dev_logger.o tz_utils.o tz_shm.o
tzd-$(CONFIG_COMPAT) += tz_compat.o
tzd-y += ../common/tz_comm.o
tzd-y += ../common/smc.o
tzd-y += ../nw_api/tz_nw_comm.o
tzd-y += ../nw_api/tz_nw_task_client.o
tzd-y += ../nw_api/tz_nw_sys_callback.o
tzd-y += ../nw_api/tz_nw_osal_linux.o
tzd-y += ../client_api/tz_client_api_linux_kernel.o
tzd-y += ../../tee/client_api/tee_client_api.o
tzd-y += ../../tee/client_api/tee_client_util.o
tzd-y += ../../tee/mgr/tee_ca_mgr_cmd.o
tzd-y += ../../tee/mgr/tee_ca_sys_cmd.o
tzd-y += ../../tee/ree/ree_sys_callback.o
tzd-y += ../../tee/ree/ree_sys_callback_linux.o

obj-m += tzlogger.o
tzlogger-y := tz_logger.o

ifeq ($(KERNELRELEASE),)
# change below kernel directory to match your system
KERNELDIR ?= ~/gtv/vendor/marvell-sdk/linux

# don't use arm-linux-androideabi-, for it's only for building android
# application. if use it to build kernel or driver, they would get some
# unexpected behaviors, which is very difficult to debug.
CROSS_COMPILE ?= arm-eabi-

ARCH ?= arm

export ARCH CROSS_COMPILE

.PHONY: all clean

all: tzd

tzd: clean
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) clean
	$(Q)rm -rf $(tzd-y)
	$(Q)rm -rf $(tzlogger-y)
else
EXTRA_CFLAGS += -I$(M)/../../tz/nw_api/
EXTRA_CFLAGS += -I$(M)/../../tz/common/
EXTRA_CFLAGS += -I$(M)/../../include/
EXTRA_CFLAGS += -I$(M)/../../arch/arm/include/
EXTRA_CFLAGS += -I$(M)/../../tz/driver/
EXTRA_CFLAGS += -I$(M)/../../tz/client_api/
EXTRA_CFLAGS += -I$(M)/../../tee/common/
EXTRA_CFLAGS += -I$(M)/../../tee/client_api/
EXTRA_CFLAGS += -I$(M)/../../tee/internal_api/
EXTRA_CFLAGS += -I$(M)/../../tee/mgr/
EXTRA_CFLAGS += -I$(M)/../../tee/ree/
EXTRA_CFLAGS += -DCONFIG_TEE
endif
