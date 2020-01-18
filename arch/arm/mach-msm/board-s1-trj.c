/* arch/arm/mach-msm/board-sapphire-wifi.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Dmitry Shmidt <dimitrysh@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <asm/mach/mmc.h>
#include <linux/gpio.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>


static void (*trj_status_cb)(int card_present, void *dev_id);
static void *trj_status_cb_devid;

static int s1_trj_status_register(void (*callback)(int card_present,
							  void *dev_id),
					 void *dev_id)
{
	if (trj_status_cb)
		return -EAGAIN;
	trj_status_cb = callback;
	trj_status_cb_devid = dev_id;
	return 0;
}

static int s1_trj_cd = 1;	/* TransferJet virtual 'card detect' status */

static unsigned int s1_trj_status(struct device *dev)
{
	return s1_trj_cd;
}

int s1_trj_set_carddetect(int val)
{
	printk(KERN_DEBUG "%s: %d\n", __func__, val);
	s1_trj_cd = val;
	if (trj_status_cb)
		trj_status_cb(val, trj_status_cb_devid);
	else
		printk(KERN_WARNING "%s: Nobody to notify\n", __func__);
	return 0;
}

EXPORT_SYMBOL(s1_trj_set_carddetect);

int s1_trj_power(int on)
{
	gpio_set_value(S1_GPIO_TRJ_EN, on);
	
	return 0;
}

struct mmc_platform_data s1_trj_data = {
	.ocr_mask		= MMC_VDD_27_28 | MMC_VDD_28_29,
	.status			= s1_trj_status,
	.register_status_notify	= s1_trj_status_register,
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};


static struct platform_device s1_trj_device = {
	.name		= "sdioJetCardDriver",
	.id 	= 0,
	//.num_resources	= ARRAY_SIZE(s1_wifi_resources),
	//.resource		= s1_wifi_resources,
	//.dev		= {
	//	.platform_data = &s1_trj_ops,
	//},
};

static int __init s1_trj_init(void)
{
	int ret;

	printk("%s: start\n", __func__);

	s1_trj_power(0);
	ret = platform_device_register(&s1_trj_device);
        return ret;
}

late_initcall(s1_trj_init);

