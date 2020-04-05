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
# __LINUX_KERNEL__ is used in OS-independent header files
MV_DEFINE := -DBERLIN -DSoC_Galois -D__LINUX_KERNEL__ -D__CODE_LINK__=0 -DCPUINDEX=0
MV_DEFINE += -DBERLIN_B_0=0x1000 \
             -DBERLIN_C_0=0x2000 \
             -DBERLIN_BG2=0x3000 \
             -DBERLIN_BG2_Z2=0x3010 \
             -DBERLIN_BG2_A0=0x3020 \
             -DBERLIN_BG2_CDP=0x3040 \
             -DBERLIN_BG3_CD=0x3040 \
             -DBERLIN_BG2_CT=0x3050 \
             -DBERLIN_BG2_Q=0x3060 \
             -DBERLIN_BG2Q_A1=0x3070 \
             -DBERLIN_BG2_DTV=0x3080 \
             -DBERLIN_BG2_DTV_A0=0x3090 \
             -DBERLIN_BG4_CD=0x30A0 \
             -DBERLIN_BG4_CT=0x30B0

MV_DEFINE += -DSOC_MINOR_REV_Z1=0x1  \
             -DSOC_MINOR_REV_Z2=0x2  \
             -DSOC_MINOR_REV_A0=0x4  \
             -DSOC_MINOR_REV_A1=0x5

###############################################################################
#
# AMP kernel module configuration
#
###############################################################################

###############################################################################
# Here defines the default AMP kernel module configuration settings
###############################################################################
CONFIG_FASTLOGO=y

# enable or disable AVIF driver
CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=y

# enable or disable AIP driver
CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=y

# enable or disable VIP driver
CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=y

# enable or disable HDMI RX driver
CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=y

# enable or disable AUDIO LoRo
CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=y

# enable or disable AUDIO SPDIF
CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=y

# enable or disable AUDIO HDMI PASSTHROUGH
CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=y

# enable or disable HW APP module
CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE=n

# enable or disable ZSP module
CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE=n

# enable or disable VMETA
CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE=y

# enable or disable V2G
CONFIG_MV_AMP_COMPONENT_V2G_ENABLE=y

# enable or disable CPM
CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=n

###############################################################################
# Now include the amp config file if any
###############################################################################
-include $(PROFILE_OUT)/config.mak

###############################################################################
# Setup the AMP kernel module configuration accordingly
###############################################################################
ifeq ($(MV_AMP_COMPONENT_AVIN_ENABLE), false)
CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=n
CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=n
CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=n
CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=n
endif

ifeq ($(MV_AMP_AUDIO_SNDSRV_PATH_71_DISABLE), true)
CONFIG_MV_AMP_AUDIO_PATH_71_ENABLE=n
endif

ifeq ($(MV_AMP_AUDIO_SNDSRV_PATH_20_DISABLE), true)
CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=n
endif

ifeq ($(MV_AMP_AUDIO_SNDSRV_PATH_SPDIF_DISABLE), true)
CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=n
endif

ifeq ($(MV_AMP_AUDIO_SNDSRV_PATH_HDMI_DISABLE), true)
CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=n
endif

###############################################################################
#
# Common AMP kernel module objects
#
###############################################################################
amp_core-objs := \
	amp_driver.o \
	drv_msg.o \
	drv_aout.o \
	drv_vpu.o \
	drv_vpp.o \
	drv_morpheus.o

###############################################################################
#
# Chip specific section
#
###############################################################################
# define BERLIN_CHIP_VERSION
ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_CT)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_CT
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_CT
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_A0)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_A0
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_A0
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_CDP)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_CDP
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_CDP
        CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=y
        MV_DEFINE += -DBERLIN_CHIP_VERSION_EXT=SOC_MINOR_REV_Z2
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG3_CD)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG3_CD
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG3_CD
        CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=y
        CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=y
        MV_DEFINE += -DBERLIN_CHIP_VERSION_EXT=SOC_MINOR_REV_A0
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_Q)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_Q
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_Q
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2Q_A1)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2Q_A1
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_Q
        MV_DEFINE += -DVPP_DHUB_IN_TZ
        CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=y
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_DTV)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_DTV
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_DTV
        #MV_DEFINE += -DVPP_DHUB_IN_TZ
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_DTV_A0)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2_DTV_A0
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_DTV_A0
        MV_DEFINE += -DVPP_DHUB_IN_TZ
        CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=y
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CD)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG4_CD
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG4_CD
        CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_V2G_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=n
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CT)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG4_CT
        FIRMWARE_INC_DIR_LOCAL=$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG4_CT
        CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_AIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_VIP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE=y
        CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE=n
        CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE=y
        CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_V2G_ENABLE=n
        CONFIG_MV_AMP_COMPONENT_CPM_ENABLE=n
endif
###############################################################################
#
# Collect the configurations and generate the build rules
#
###############################################################################

ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_AVIN_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_AIP_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_VIP_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_VIP_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_HDMIRX_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_AUDIO_PATH_20_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_AUDIO_PATH_20_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_AUDIO_PATH_SPDIF_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_AUDIO_PATH_HDMI_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_ZSP_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_VMETA_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_VMETA_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_V2G_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_V2G_ENABLE
endif

ifeq ($(CONFIG_MV_AMP_COMPONENT_CPM_ENABLE), y)
        MV_DEFINE += -DCONFIG_MV_AMP_COMPONENT_CPM_ENABLE
endif
###############################################################################
#
# Common CPM kernel module objects
#
###############################################################################
ifeq ($(CONFIG_MV_AMP_COMPONENT_CPM_ENABLE), y)
CPM_ABS_PATH := $(SUBDIRS)/cpm
CPM_API_OBJS := cpm/source/cpm_driver.o

CPM_INC_PATH := -I$(CPM_ABS_PATH)/include

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG3_CD)
        CPM_API_OBJS += cpm/source/cpm_bg3cd.o
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2Q_A1)
        CPM_API_OBJS += cpm/source/cpm_bg2q.o
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_DTV_A0)
        CPM_API_OBJS += cpm/source/cpm_bg2dtv.o
endif
endif
###############################################################################
#
# Common DHUB kernel module objects
#
###############################################################################
DHUB_ABS_PATH := $(SUBDIRS)/dhub
DHUB_API_OBJS := dhub/source/avio_dhub_drv.o    \
                 dhub/source/hal_dhub.o         \
                 dhub/source/hal_dhub_wrap.o
DHUB_CA_OBJS  := dhub/ca/tee_ca_dhub.o
DHUB_INC_PATH := -I$(DHUB_ABS_PATH)/include
DHUB_CFG_DIR  := dhub/configs
####  add soc specific dhub cfg###############################################
ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_CDP)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg3cd/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg3cd/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg3cd\
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_CDP
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG3_CD)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg3cd/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg3cd/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg3cd\
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG3_CD
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_CT)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg2ct/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg2ct/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg2ct \
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_CT
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_Q)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg2q/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg2q/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg2q \
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_Q
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2Q_A1)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg2q/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg2q/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg2q \
        -I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_Q
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_DTV)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg2dtv/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg2dtv/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg2dtv \
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_DTV
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG2_DTV_A0)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg2q4k/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg2q4k/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg2q4k \
	-I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG2_DTV_A0
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CD)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg4cd/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg4cd/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg4cd \
        -I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG4_CD
endif

ifeq ($(MV_BERLIN_CHIP_VERSION), BERLIN_BG4_CT)
DHUB_CFG_OBJS := $(DHUB_CFG_DIR)/bg4ct/avio_dhub_cfg.o
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
DHUB_CFG_OBJS += $(DHUB_CFG_DIR)/bg4ct/avif_dhub_cfg.o
endif
DHUB_INC_PATH += -I$(DHUB_ABS_PATH)/configs/bg4ct \
        -I$(GS_INC_DIR)/../../../hal/common/include/Firmware_Berlin_BG4_CT
endif

DHUB_ALL_OBJS := $(DHUB_API_OBJS)   \
                 $(DHUB_CA_OBJS)    \
                 $(DHUB_CFG_OBJS)

CPM_ALL_OBJS := $(CPM_API_OBJS)

amp_core-objs += \
    $(DHUB_ALL_OBJS)\
    $(CPM_ALL_OBJS)

###############################################################################
#
# Configurable kernel module objects
#
###############################################################################
ifeq ($(CONFIG_MV_AMP_COMPONENT_HWAPP_ENABLE), y)
amp_core-objs += drv_app.o
endif
ifeq ($(CONFIG_MV_AMP_COMPONENT_ZSP_ENABLE), y)
amp_core-objs += drv_zsp.o
endif
ifeq ($(CONFIG_MV_AMP_COMPONENT_AVIN_ENABLE), y)
amp_core-objs += drv_avif.o
else
ifeq ($(CONFIG_MV_AMP_COMPONENT_AIP_ENABLE), y)
amp_core-objs += drv_avif.o
endif
endif

HEADER_PATH +=  \
    -I$(srctree)/arch/arm/mach-berlin/modules/shm \
    -I$(srctree)/arch/arm/mach-berlin/modules/cc  \
    -I$(FIRMWARE_INC_DIR_LOCAL)  \
    -I$(GS_INC_DIR) \
    $(DHUB_INC_PATH)\
    $(CPM_INC_PATH)

ccflags-y += $(MV_DEFINE) $(HEADER_PATH)

ccflags-y += -DBERLIN_SINGLE_CPU

#ifeq ($(CONFIG_FASTLOGO),y)
ccflags-y += -DBERLIN_BOOTLOGO
#endif

MODULE_CFLAGS := -I$(TEE_ROOT_DIR)/tee/tee/client_api \
                 -I$(TEE_ROOT_DIR)/tee/tee/common \
                 -I$(TEE_ROOT_DIR)/tee/include
ccflags-y += $(MODULE_CFLAGS)
#ccflags-y += -Werror

obj-m += amp_core.o
