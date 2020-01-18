#
# Makefile fragment for Linux 2.6
# Broadcom DHD Driver
#
# Copyright (C) 1999-2010, Broadcom Corporation
# 
#      Unless you and Broadcom execute a separate written software license
# agreement governing use of this software, this software is licensed to you
# under the terms of the GNU General Public License version 2 (the "GPL"),
# available at http://www.broadcom.com/licenses/GPLv2.php, with the
# following added to such license:
# 
#      As a special exception, the copyright holders of this software give you
# permission to link this software with independent modules, and to copy and
# distribute the resulting executable under terms of your choice, provided that
# you also meet, for each linked independent module, the terms and conditions of
# the license of that module.  An independent module is a module which is not
# derived from this software.  The special exception does not apply to any
# modifications of the software.
# 
#      Notwithstanding the above, under no circumstances may you combine this
# software in any way with any other Broadcom software provided under a license
# other than the GPL, without Broadcom's express prior written consent.
#
# $Id: makefile.26,v 1.1.144.1 2008/05/07 22:53:44 Exp $

SRCBASE := drivers/net/wireless/bcm4329

WLANCFLAGS = -DLINUX -DSRCBASE=\"$(SRCBASE)\" -DBCMDRIVER -DBCMDONGLEHOST \
	-DDHDTHREAD -DBCMWPA2 -DBCMWAPI_WPI -DUNRELEASEDCHIP -Dlinux      \
	-DDHD_SDALIGN=64 -DMAX_HDR_READ=64 -DDHD_FIRSTREAD=64 -DDHD_GPL   \
	-DDHD_SCHED -DBDC -DTOE -DDHD_BCMEVENTS -DSHOW_EVENTS -DBCMSDIO   \
	-DBCMLXSDMMC -DBCMPLATFORM_BUS -DCUSTOMER_HW2 \
	-DDHD_USE_STATIC_BUF -DEMBEDDED_PLATFORM -DARP_OFFLOAD_SUPPORT    \
	-DPKT_FILTER_SUPPORT -DSOFTAP -fshort-wchar -Wall                 \
	-Wstrict-prototypes -Werror -I$(SRCBASE) -I$(SRCBASE)/include

ifeq ($(CONFIG_BCM4329_DEBUG),y)
WLANCFLAGS += -DDHD_DEBUG -DSDTEST
endif
WLANCFLAGS += -DOOB_INTR_ONLY -DMMC_SDIO_ABORT -DMACADDR_FROM_NV_ITEM  \
	-DCONFIG_PRESCANNED -DGET_CUSTOM_MAC_ENABLE

WLANOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_common.o dhd_sdio.o siutils.o \
	bcmsdh.o bcmutils.o dhd_custom_gpio.o hndpmu.o wl_iw.o bcmsdh_linux.o \
	bcmwifi.o dhd_linux.o linux_osl.o bcmsdh_sdmmc.o dhd_cdc.o sbutils.o  \
	dhd_linux_sched.o

obj-$(CONFIG_BCM4329) += bcm4329.o
bcm4329-objs = $(WLANOFILES)
EXTRA_CFLAGS = $(WLANCFLAGS)
EXTRA_LDFLAGS += --strip-debug
