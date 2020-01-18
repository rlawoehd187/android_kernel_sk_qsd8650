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
#include <linux/skbuff.h>
#include <linux/wifi_tiwlan.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <asm/mach/mmc.h>
#include <linux/gpio.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;


static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *s1_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init s1_init_wifi_mem (void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}

extern int s1_wifi_power(int on);

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static int s1_wifi_status_register(void (*callback)(int card_present,
							  void *dev_id),
					 void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int s1_wifi_cd = 0;	/* WIFI virtual 'card detect' status */

static unsigned int s1_wifi_status(struct device *dev)
{
	return s1_wifi_cd;
}

int s1_wifi_set_carddetect(int val)
{
	printk(KERN_DEBUG "%s: %d\n", __func__, val);
	s1_wifi_cd = val;
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		printk(KERN_WARNING "%s: Nobody to notify\n", __func__);
	return 0;
}

#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(s1_wifi_set_carddetect);
#endif

static int s1_wifi_reset_state;
int s1_wifi_reset(int on)
{
	printk(KERN_DEBUG "%s: %d\n", __func__, on);
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
	gpio_set_value(S1_GPIO_WIFI_RST, !on);
#endif
	s1_wifi_reset_state = on;
	mdelay(50);

	return 0;
}
#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(s1_wifi_reset);
#endif


/* BCM4329 returns wrong sdio_vsn(1) when we read cccr,
 * we use predefined value (sdio_vsn=2) here to initial sdio driver well
 */
static struct embedded_sdio_data s1_wifi_emb_data = {
	.cccr	= {
		.sdio_vsn	= 2,
		.multi_block	= 1,
		.low_speed	= 0,
		.wide_bus	= 0,
		.high_power	= 1,
		.high_speed	= 1,
	},
};


struct mmc_platform_data s1_wifi_data = {
	.ocr_mask		= MMC_VDD_28_29,
	.status			= s1_wifi_status,
	.register_status_notify	= s1_wifi_status_register,
	.embedded_sdio		= &s1_wifi_emb_data,
//	.sdiowakeup_irq	= MSM_GPIO_TO_INT(S1_GPIO_WIFI_HOST_WAKE)
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};
//

static struct resource s1_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= MSM_GPIO_TO_INT(S1_GPIO_WIFI_HOST_WAKE),
		.end		= MSM_GPIO_TO_INT(S1_GPIO_WIFI_HOST_WAKE),
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

struct wifi_platform_data s1_wifi_control = {
	.set_power		= s1_wifi_power,
	.set_reset		= s1_wifi_reset,
	.set_carddetect		= s1_wifi_set_carddetect,
	.mem_prealloc		= s1_wifi_mem_prealloc,
};

static struct platform_device s1_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
        .num_resources  = ARRAY_SIZE(s1_wifi_resources),
        .resource       = s1_wifi_resources,
	.dev		= {
		.platform_data = &s1_wifi_control,
	},
};


static int __init s1_wifi_init(void)
{
	int ret;

	printk("%s: start\n", __func__);
//	mahimahi_wifi_update_nvs("sd_oobonly=1\r\n");
	s1_init_wifi_mem();
	ret = platform_device_register(&s1_wifi_device);
        return ret;
}

late_initcall(s1_wifi_init);






#if defined(CONFIG_DEBUG_FS)
static int s1_dbg_wifi_reset_set(void *data, u64 val)
{
	s1_wifi_reset((int) val);
	return 0;
}

static int s1_dbg_wifi_reset_get(void *data, u64 *val)
{
	*val = s1_wifi_reset_state;
	return 0;
}

static int s1_dbg_wifi_cd_set(void *data, u64 val)
{
	s1_wifi_set_carddetect((int) val);
	return 0;
}

static int s1_dbg_wifi_cd_get(void *data, u64 *val)
{
	*val = s1_wifi_cd;
	return 0;
}

int s1_wifi_power_state=0;
static int s1_dbg_wifi_pwr_set(void *data, u64 val)
{
	s1_wifi_power((int) val);
	s1_wifi_power_state = val;
	return 0;
}

static int s1_dbg_wifi_pwr_get(void *data, u64 *val)
{

	*val = s1_wifi_power_state;
	return 0;
}


DEFINE_SIMPLE_ATTRIBUTE(s1_dbg_wifi_reset_fops,
			s1_dbg_wifi_reset_get,
			s1_dbg_wifi_reset_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(s1_dbg_wifi_cd_fops,
			s1_dbg_wifi_cd_get,
			s1_dbg_wifi_cd_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(s1_dbg_wifi_pwr_fops,
			s1_dbg_wifi_pwr_get,
			s1_dbg_wifi_pwr_set, "%llu\n");

static int __init s1_dbg_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("s1_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("wifi_reset", 0644, dent, NULL,
			    &s1_dbg_wifi_reset_fops);
	debugfs_create_file("wifi_cd", 0644, dent, NULL,
			    &s1_dbg_wifi_cd_fops);
	debugfs_create_file("wifi_pwr", 0644, dent, NULL,
			    &s1_dbg_wifi_pwr_fops);

	return 0;
}

device_initcall(s1_dbg_init);
#endif

