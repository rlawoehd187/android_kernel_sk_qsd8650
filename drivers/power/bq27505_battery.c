/*
 * BQ275xx battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/earlysuspend.h>
#include <linux/i2c/bq27505_battery.h>
#include <linux/wakelock.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>


#undef BQ27505_OLD_STYLE
#undef BQ27505_PROC_TEST

#ifdef CONFIG_MACH_QSD8X50_S1
extern void msm_batt_update_psy_status(void);
#endif // CONFIG_MACH_QSD8X50_S1

unsigned char skts_s1_0215_20100823_configured[1024] = {
	0x32, 0x31, 0x04, 0x00, 0xE0, 0x28, 0xF8, 0x03, 0x8A, 0xA3, 0x1A, 0x37,
	0x03, 0x44, 0xA0, 0x06, 0xB8, 0x04, 0x00, 0xE0, 0x03, 0x5C, 0x03, 0x66,
	0xA0, 0x04, 0x24, 0x03, 0x28, 0xA0, 0x7F, 0x3B, 0x81, 0x50, 0x93, 0x55,
	0x56, 0xD4, 0xFA, 0x45, 0x00, 0x4C, 0x0E, 0x0C, 0x00, 0x00, 0x0B, 0xB8,
	0x10, 0x68, 0x0B, 0xA4, 0x03, 0xE8, 0x00, 0x20, 0x03, 0xE8, 0x00, 0x20,
	0x00, 0x20, 0x10, 0x96, 0x00, 0xD4, 0x86, 0x4A, 0xC6, 0xB4, 0xC2, 0x6E,
	0x2B, 0x03, 0x7C, 0x01, 0x48, 0xFD, 0xA3, 0xF6, 0x75, 0x12, 0x58, 0x2D,
	0xB7, 0x2C, 0x4A, 0x00, 0x00, 0x00, 0x00, 0xCF, 0xDC, 0x17, 0xF3, 0x00,
	0x00, 0x17, 0xF3, 0xEF, 0x03, 0x11, 0x05, 0x01, 0x00, 0x00, 0x10, 0x01,
	0x00, 0x3C, 0x00, 0x50, 0x3C, 0x00, 0x64, 0x3C, 0x00, 0x20, 0x00, 0x00,
	0x05, 0x62, 0x00, 0x00, 0x02, 0x05, 0x62, 0x00, 0x00, 0x02, 0x00, 0x0B,
	0x00, 0x0B, 0xFE, 0xD5, 0xFB, 0x95, 0x00, 0x02, 0x02, 0xBC, 0x01, 0x01,
	0x02, 0xBC, 0x01, 0x2C, 0x00, 0x1E, 0x00, 0xC8, 0xC8, 0x14, 0x08, 0x00,
	0x3C, 0x0E, 0x10, 0x00, 0x0A, 0x46, 0x05, 0x14, 0x05, 0x0F, 0x0A, 0x03,
	0x20, 0x00, 0x64, 0x46, 0x50, 0x28, 0x0E, 0xDD, 0x0E, 0xA2, 0x01, 0x90,
	0x00, 0x64, 0x19, 0x01, 0x03, 0x05, 0x05, 0x25, 0x5A, 0x0F, 0x1E, 0x60,
	0x0C, 0x80, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x43, 0x80, 0x04, 0x01, 0x32, 0x05, 0x62, 0x02, 0x10, 0x5B, 0xE5,
	0xE4, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC, 0xEE, 0xEF, 0xF0, 0xF1, 0xF0,
	0xF0, 0xF1, 0xF3, 0xEF, 0xEB, 0xF3, 0xF4, 0xF8, 0xF9, 0xFC, 0xFB, 0xFD,
	0xFD, 0xFE, 0x00, 0xFF, 0xF7, 0xF5, 0xEF, 0xEE, 0xEF, 0xF2, 0xFC, 0xFF,
	0xF1, 0xFC, 0x00, 0x94, 0xE1, 0x1B, 0xC5, 0x0A, 0xF7, 0x5F, 0x77, 0xF9,
	0x5F, 0x96, 0xF9, 0x1F, 0x94, 0xF9, 0xBF, 0x94, 0xF9, 0xDF, 0x91, 0xF9,
	0x6F, 0x8E, 0xF8, 0xFF, 0xA9, 0xFD, 0xBF, 0xFE, 0xFF, 0xFF, 0xDA, 0xFA,
	0x8F, 0x75, 0xF6, 0x8F, 0x6A, 0xF6, 0x6F, 0x44, 0xF3, 0xDF, 0x28, 0xF1,
	0xEF, 0x11, 0xEC, 0xAE, 0x86, 0xE7, 0xDE, 0x7D, 0xE9, 0x8E, 0xBD, 0xEB,
	0x6E, 0xB8, 0xEA, 0x6C, 0x5B, 0xB5, 0x00, 0x00, 0x00, 0x55, 0x01, 0x1B,
	0x01, 0xB0, 0xFE, 0x02, 0xFC, 0xFE, 0x08, 0x26, 0x04, 0x08, 0x02, 0x00,
	0x1A, 0x50, 0x72, 0x00, 0x00, 0xFE, 0xCA, 0x00, 0x00, 0x17, 0x1E, 0x10,
	0xCC, 0xFD, 0xFE, 0x01, 0x0D, 0xFB, 0xE3, 0xE6, 0x47, 0x48, 0x01, 0x32,
	0x05, 0x62, 0x02, 0x10, 0x5B, 0xE5, 0xE4, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB,
	0xEC, 0xEE, 0xEF, 0xF0, 0xF1, 0xF0, 0xF0, 0xF1, 0xF3, 0xEF, 0xEB, 0xF3,
	0xF4, 0xF8, 0xF9, 0xFC, 0xFB, 0xFD, 0xFD, 0xFE, 0x00, 0xFF, 0xF7, 0xF5,
	0xEF, 0xEE, 0xEF, 0xF2, 0xFC, 0xFF, 0xF1, 0xFC, 0x00, 0x94, 0xE1, 0x1B,
	0xC5, 0x0A, 0xF7, 0x5F, 0x77, 0xF9, 0x5F, 0x96, 0xF9, 0x1F, 0x94, 0xF9,
	0xBF, 0x94, 0xF9, 0xDF, 0x91, 0xF9, 0x6F, 0x8E, 0xF8, 0xFF, 0xA9, 0xFD,
	0xBF, 0xFE, 0xFF, 0xFF, 0xDA, 0xFA, 0x8F, 0x75, 0xF6, 0x8F, 0x6A, 0xF6,
	0x6F, 0x44, 0xF3, 0xDF, 0x28, 0xF1, 0xEF, 0x11, 0xEC, 0xAE, 0x86, 0xE7,
	0xDE, 0x7D, 0xE9, 0x8E, 0xBD, 0xEB, 0x6E, 0xB8, 0xEA, 0x6C, 0x5B, 0xB5,
	0x00, 0x00, 0x00, 0x55, 0x01, 0x1B, 0x01, 0xB0, 0xFE, 0x02, 0xFC, 0xFE,
	0x08, 0x26, 0x04, 0x08, 0x02, 0x00, 0x1A, 0x50, 0x72, 0x00, 0x00, 0xFE,
	0xCA, 0x00, 0x00, 0x17, 0x1E, 0x10, 0xCC, 0xFD, 0xFE, 0x01, 0x0D, 0xFB,
	0xE3, 0xE6, 0x47, 0x48, 0x00, 0x00, 0x01, 0xC2, 0x00, 0x64, 0x00, 0xC8,
	0x10, 0x68, 0x00, 0x32, 0xFF, 0xCE, 0x02, 0x26, 0x00, 0x00, 0x00, 0x64,
	0x00, 0x19, 0x00, 0x64, 0x28, 0x63, 0x5F, 0x64, 0x62, 0x81, 0x72, 0x04,
	0x2E, 0x00, 0x6E, 0x14, 0x01, 0x04, 0x00, 0x00, 0x50, 0x00, 0x14, 0x0A,
	0xF0, 0x06, 0xD6, 0x00, 0x0B, 0xB8, 0x00, 0x0F, 0x05, 0x00, 0x32, 0x01,
	0xC2, 0x14, 0x14, 0x00, 0x08, 0x09, 0xF6, 0x00, 0x3C, 0x00, 0x4B, 0x00,
	0x14, 0x00, 0x3C, 0x3C, 0x01, 0x36, 0x72, 0x04, 0x14, 0xFF, 0xFF, 0xFF,
	0xFF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA,
	0x98, 0x76, 0x54, 0x32, 0x10, 0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB,
	0x89, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x54, 0x76, 0xC3, 0xD2, 0xE1,
	0xF0, 0x02, 0x26, 0x02, 0x01, 0xF4, 0x02, 0x58, 0x02, 0x02, 0x26, 0x00,
	0x00, 0x00, 0x00, 0xA9, 0xFE, 0x1D, 0x6B, 0x01, 0xF4, 0x00, 0x64, 0x01,
	0xB0, 0xF6, 0xFE, 0x0C, 0x04, 0xDA, 0x5A, 0x05, 0x64, 0xFE, 0x70, 0x0B,
	0xA6, 0x07, 0x62, 0x71, 0x32, 0x37, 0x35, 0x30, 0x35, 0x96, 0xA0, 0x4B,
	0x64, 0x32, 0x0C, 0x80, 0x02, 0x0C, 0xE4, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x0B, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x55, 0x01, 0x1B, 0x01, 0xB0, 0xFE, 0x02, 0xFC, 0xFE, 0x08, 0x26,
	0x04, 0x08, 0x02, 0x00, 0x1A, 0x50, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x01, 0x1B,
	0x01, 0xB0, 0xFE, 0x02, 0xFC, 0xFE, 0x08, 0x26, 0x04, 0x08, 0x02, 0x00,
	0x1A, 0x50, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x01, 0x1B, 0x01, 0xB0, 0xFE, 0x02,
	0xFC, 0xFE, 0x08, 0x26, 0x04, 0x08, 0x02, 0x00, 0x1A, 0x50, 0x72, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0x01, 0x1B, 0x01, 0xB0, 0xFE, 0x02, 0xFC, 0xFE, 0x08, 0x26,
	0x04, 0x08, 0x02, 0x00, 0x1A, 0x50, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x31, 0x04, 0x00,
	0xE0, 0x28, 0xF8, 0x03, 0x8A, 0xA3, 0x1A, 0x37, 0x03, 0x44, 0xA0, 0x06,
	0xB8, 0x04, 0x00, 0xE0, 0x03, 0x5C, 0x03, 0x66, 0xA0, 0x04, 0x24, 0x03,
	0x28, 0xA0, 0x7F, 0x3B, 0x81, 0x50, 0x93, 0x55, 0x56, 0xD4, 0xFA, 0x45,
	0x00, 0x4C, 0x0E, 0x0C, 0x00, 0x00, 0x0B, 0xB8, 0x10, 0x68, 0x0B, 0xA4,
	0x03, 0xE8, 0x00, 0x20, 0x03, 0xE8, 0x00, 0x20, 0x00, 0x20, 0x10, 0x96,
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x0D, 0x22, 0xFF, 0xFF, 0xF2, 0xDC,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF
};


#define BQ27505_SUB_ROM_MODE         0x0F00
#define BQ27505_I2C_ADDR_FOR_DFI     0x0B

#define HIBYTE(w)		((unsigned char)(((unsigned short)(w) >> 8) & 0xFF))
#define LOBYTE(w)		((unsigned char)(w))

struct bq27505_device_info {
	struct device           *dev;
	int                     pwr_pin;
	int                     pwr_sts;
	int                     irq;
	bool                    irq_enabled;
	int                     is_updating_dfi;
	struct i2c_client       *client;
	struct work_struct      isr_work;
#ifdef BQ27505_OLD_STYLE
	struct delayed_work     resume_work;
#endif
	struct wake_lock        wake_lock;
#if 0//def CONFIG_HAS_EARLYSUSPEND
	struct early_suspend    early_suspend;
#endif
};


/*
 * Primitive Prototype for BQ27505 devices
 */

static int bq27505_read(u8 reg, unsigned char *buf, unsigned char len,
			struct bq27505_device_info *di);
#ifdef BQ27505_PROC_TEST
static int bq27505_write(u8 reg, unsigned char *buf, unsigned char len,
			struct bq27505_device_info *di);
#endif

static struct bq27505_device_info *g_di;
static bool bq27505_probed = false;
static DEFINE_MUTEX(bq27505_mutex);


/*
 * External code for BQ27505 devices
 */

bool bq27505_battery_probed(void)
{
	return bq27505_probed;
}
EXPORT_SYMBOL(bq27505_battery_probed);


int bq27505_battery_property(bq27505_std_cmd cmd, int *value)
{
	int ret;
	unsigned char buf[2];

	if (!bq27505_probed || !g_di || g_di->is_updating_dfi || !g_di->pwr_sts)
		return -1;

	mutex_lock(&bq27505_mutex);

	switch (cmd)
	{
		case BQ27505_STD_TEMP:
		case BQ27505_STD_VOLT:
		case BQ27505_STD_NAC:
		case BQ27505_STD_FAC:
		case BQ27505_STD_RM:
		case BQ27505_STD_FCC:
		case BQ27505_STD_AI:
		case BQ27505_STD_TTE:
		case BQ27505_STD_TTF:
		case BQ27505_STD_SOH:
		case BQ27505_STD_SOC:
		case BQ27505_STD_NIC:
			ret = bq27505_read(cmd, buf, 2, g_di);
			if (ret) {
				dev_err(&g_di->client->dev, "%s - error reading property(cmd:0x%X) \n",
					__func__, cmd);
				break;
			}
			*value = (buf[1] << 8) | buf[0];
			if (cmd == BQ27505_STD_AI)
				*value = (short)*value;
			else if (cmd == BQ27505_STD_SOH)
				*value = buf[0];
			else if (cmd == BQ27505_STD_SOC && *value > 100)
				*value = 100;
			else if (cmd == BQ27505_STD_TEMP)
				*value = *value - 2730;
			break;
		default:
			dev_err(&g_di->client->dev, "%s - Not Supported(cmd:0x%X) \n",
				__func__, cmd);
			ret = -1;
			break;
	}

	mutex_unlock(&bq27505_mutex);

	return ret;
}
EXPORT_SYMBOL(bq27505_battery_property);


void bq27505_battery_interrupt(bool enable)
{
	if (!g_di)
		return;
	if (!!enable == !!g_di->irq_enabled)
		return;
	if (enable)
	{
		g_di->irq_enabled = true;
		enable_irq(g_di->irq);
	}
	else
	{
		g_di->irq_enabled = false;
		disable_irq(g_di->irq);
	}
}
EXPORT_SYMBOL(bq27505_battery_interrupt);


/*
 * Common code for BQ27505 devices
 */

static int bq27505_read(u8 reg, unsigned char *buf, unsigned char len,
			struct bq27505_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0) {
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = buf;
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0)
			return 0;
	}
	return err;
}

#ifdef BQ27505_PROC_TEST
static int bq27505_write(u8 reg, unsigned char *buf, unsigned char len,
			struct bq27505_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[100];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1 + len;
	msg->buf = data;

	data[0] = reg;
	memcpy(&data[1], buf, len);
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;
	return err;
}
#endif

static int bq27505_write_sub_command(int cntl_data, 
			struct bq27505_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[3];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = data;

	data[0] = BQ27505_STD_CNTL;
	data[1] = LOBYTE(cntl_data);
	data[2] = HIBYTE(cntl_data);
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;

	return err;
}

static int bq27505_read_sub_command(int cntl_data,
			int *rt_value, struct bq27505_device_info *di)
{
	int err;
	unsigned char buf[2];

	err = bq27505_write_sub_command(cntl_data, di);
	if (err == 0) {
		err = bq27505_read(BQ27505_STD_CNTL, buf, 2, di);
		*rt_value = (buf[1] << 8) | buf[0];
		if (err == 0)
			return 0;
	}

	return err;
}


/*
 * Specific Code for BQ27505 devices - DFI Update
 */

static int bq27505_read_for_dfi(u8 reg, unsigned char *buf,
			unsigned char len, struct bq27505_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = BQ27505_I2C_ADDR_FOR_DFI;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0) {
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = buf;
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0)
			return 0;
	}
	return err;
}

static int bq27505_write_multi_for_dfi(u8 reg, unsigned char *buf,
			unsigned char len, struct bq27505_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[100];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = BQ27505_I2C_ADDR_FOR_DFI;
	msg->flags = 0;
	msg->len = 1 + len;
	msg->buf = data;

	data[0] = reg;
	memcpy(&data[1], buf, len);
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return 0;
	return err;
}

static int bq27505_write_for_dfi(u8 reg, unsigned char buf,
			struct bq27505_device_info *di)
{
	unsigned char data = buf;

	return bq27505_write_multi_for_dfi(reg, &data, 1, di);
}


#define BQ27505_I2C_RETRY_COUNT 5

static int bq27505_update_dfi(void)
{
	int ret, err, iRow, iCol; 
	int iRetry, retry1_cnt, retry2_cnt;
	unsigned char *yDataFlashImage, *yRowData;
	unsigned char yIFRowData[2][96], buf;
	unsigned int Checksum, DFI_Checksum, DFI_Checksum_RB;

	ret = -1;

	dev_info(&g_di->client->dev, "%s !!!\n", __func__);

	/* Update DFI Image */
	yDataFlashImage = skts_s1_0215_20100823_configured;

	/* Start Read and Erase First Tow Rows of Instruction Flash(IF) */

	err = bq27505_write_sub_command(BQ27505_SUB_ROM_MODE, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "ROM_MODE error \n");
		goto FAILURE;
	}

	for (iRow = 0; iRow < 2; iRow++)
	{
		// Read first two rows of IF
		err = bq27505_write_for_dfi(0x00, 0x00, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 1 \n");
			goto FAILURE;
		}
		// Set IF row address to 0x00 + iRow
		err = bq27505_write_for_dfi(0x01, 0x00 + iRow, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 2 \n");
			goto FAILURE;
		}
		// Set IF column address to 0  and set the data to be read as 96 byte
		err = bq27505_write_for_dfi(0x02, 0x00, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 3 \n");
			goto FAILURE;
		}
		// Checksum consists of 0x00 + iRow + 0x00
		err = bq27505_write_for_dfi(0x64, iRow, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 4 \n");
			goto FAILURE;
		}
		// Checksum required to complete read command
		err = bq27505_write_for_dfi(0x65, 0x00, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 5 \n");
			goto FAILURE;
		}
		// Read 96 bytes from data register address 0x04 to 0x63
		memset(yIFRowData[iRow], 0x0, 96);
		for (iCol = 0; iCol < 96; iCol++)
		{
			err = bq27505_read_for_dfi(0x04 + iCol, &yIFRowData[iRow][iCol], 1, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 1st stage - 6(idx-%d) \n", iCol);
				goto FAILURE;
			}
		}
		mdelay(20);
	}

	for (iRetry = 0; iRetry < BQ27505_I2C_RETRY_COUNT; iRetry++)
	{
		// Erase the first two rows(one page erase)
		err = bq27505_write_for_dfi(0x00, 0x03, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 7 \n");
			goto FAILURE;
		}
		// Checksum consists of 0x03
		err = bq27505_write_for_dfi(0x64, 0x03, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 8 \n");
			goto FAILURE;
		}
		// Checksum required to complete erase command
		err = bq27505_write_for_dfi(0x65, 0x00, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 9 \n");
			goto FAILURE;
		}
		mdelay(20);

		buf = 0xFF;
		err = bq27505_read_for_dfi(0x66, &buf, 1, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 1st stage - 10 \n");
			goto FAILURE;
		}
		if (buf == 0x0)
			break;
	}

	if (buf != 0x0)
	{
		err = -1;
		dev_err(&g_di->client->dev, "error 1st stage - 11 \n");
		goto FAILURE;
	}
	dev_err(&g_di->client->dev, "1st stage complete (retry cnt = %d) \n", iRetry);


	/* Start Writing Image */

	retry1_cnt = 0;
RETRY_1:
	DFI_Checksum = 0;

	retry2_cnt = 0;
RETRY_2:
	for (iRetry = 0; iRetry < BQ27505_I2C_RETRY_COUNT; iRetry++)
	{
		// Send data flash Mass Erase command
		err = bq27505_write_for_dfi(0x00, 0x0C, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 1 \n");
			goto WRITE_IF;
		}
		// Setup for Mass Erase command
		err = bq27505_write_for_dfi(0x04, 0x83, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 2 \n");
			goto WRITE_IF;
		}
		// Setup for Mass Erase command
		err = bq27505_write_for_dfi(0x05, 0xDE, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 3 \n");
			goto WRITE_IF;
		}
		// Checksum consists of 0x0C + 0x83 + 0xDE
		err = bq27505_write_for_dfi(0x64, 0x6D, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 3 \n");
			goto WRITE_IF;
		}
		// Checksum required to complete Mass Erase command
		err = bq27505_write_for_dfi(0x65, 0x01, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 4 \n");
			goto WRITE_IF;
		}

		mdelay(200);

		buf = 0xFF;
		err = bq27505_read_for_dfi(0x66, &buf, 1, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 5 \n");
			goto WRITE_IF;
		}
		if (buf == 0x0)
			break;
	}

	if (buf != 0x0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 6 \n");
		goto WRITE_IF;
	}
	dev_err(&g_di->client->dev, "2nd stage(1/3) complete (retry cnt = %d) \n", iRetry);

	for (iRow = 0; iRow < 32; iRow++)
	{
		yRowData = &yDataFlashImage[32 * iRow];

		// Program Row command
		err = bq27505_write_for_dfi(0x00, 0x0A, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 7 \n");
			goto WRITE_IF;
		}
		// This command indicates the row to operate in
		err = bq27505_write_for_dfi(0x01, iRow, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 8 \n");
			goto WRITE_IF;
		}
		// Write Row command
		err = bq27505_write_multi_for_dfi(0x04, yRowData, 32, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 9 \n");
			goto WRITE_IF;
		}
		Checksum = 0;
		for (iCol = 0; iCol < 32; iCol++)
			Checksum += yRowData[iCol];
		DFI_Checksum = (DFI_Checksum + Checksum) & 0xFFFF;
		Checksum = (0x0A + iRow + Checksum) & 0xFFFF;

//		dev_info(&g_di->client->dev, "Checksum 0x%X, 0x%X, 0x%X \n",
//				Checksum, LOBYTE(Checksum), HIBYTE(Checksum));
		// LSB of Checksum
		err = bq27505_write_for_dfi(0x64, LOBYTE(Checksum), g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 10 \n");
			goto WRITE_IF;
		}
		// MSB of Checksum, Checksum is required to complete Write Row command
		err = bq27505_write_for_dfi(0x65, HIBYTE(Checksum), g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 11 (iRow=%d) \n", iRow);
			goto WRITE_IF;
		}

		mdelay(2);

		buf = 0xFF;
		err = bq27505_read_for_dfi(0x66, &buf, 1, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 12 \n");
			goto WRITE_IF;
		}
		if (buf != 0)
		{
			dev_err(&g_di->client->dev, "error 2nd stage - 13 \n");
			if (++retry2_cnt >= BQ27505_I2C_RETRY_COUNT)
				goto WRITE_IF;
			goto RETRY_2;
		}
	}
	dev_err(&g_di->client->dev, "2nd stage(2/3) complete (retry cnt = %d) \n", retry2_cnt);

	// Setup for Data Flash Checksum commad
	err = bq27505_write_for_dfi(0x00, 0x08, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 14 \n");
		goto WRITE_IF;
	}

	err = bq27505_write_for_dfi(0x64, 0x08, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 15 \n");
		goto WRITE_IF;
	}

	err = bq27505_write_for_dfi(0x65, 0x00, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 16 \n");
		goto WRITE_IF;
	}

	mdelay(20);

	DFI_Checksum_RB = 0;
	err = bq27505_read_for_dfi(0x04, &buf, 1, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 17 \n");
		goto WRITE_IF;
	}
//	dev_err(&g_di->client->dev, "04-value = 0x%X \n", buf);
	DFI_Checksum_RB = buf;

	err = bq27505_read_for_dfi(0x05, &buf, 1, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 2nd stage - 18 \n");
		goto WRITE_IF;
	}
//	dev_err(&g_di->client->dev, "05-value = 0x%X \n", buf);
	DFI_Checksum_RB += (buf << 8);

	if (DFI_Checksum != DFI_Checksum_RB)
	{
		dev_err(&g_di->client->dev, "DFI_Checksum=0x%X, DFI_Checksum_RB=0x%X \n",
				DFI_Checksum, DFI_Checksum_RB);
		dev_err(&g_di->client->dev, "error 2nd stage - 19 \n");
		if (++retry1_cnt >= BQ27505_I2C_RETRY_COUNT)
			goto WRITE_IF;
		goto RETRY_1;
	}
	dev_err(&g_di->client->dev, "2nd stage(3/3) complete (retry cnt = %d)\n", retry1_cnt);

	ret = 0;

WRITE_IF:
	/* End Writing Image */

	for (iRow = 1; iRow >= 0; iRow--)
	{
		for (iRetry = 0; iRetry < BQ27505_I2C_RETRY_COUNT; iRetry++)
		{
			// Program Row command
			err = bq27505_write_for_dfi(0x00, 0x02, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 1 \n");
				goto FAILURE;
			}
			// This command indicates the row to operate in
			err = bq27505_write_for_dfi(0x01, 0x00 + iRow, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 2 \n");
				goto FAILURE;
			}
			// This command indicates the row to operate in
			err = bq27505_write_for_dfi(0x02, 0x00, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 3 \n");
				goto FAILURE;
			}
			// Write Row command
			err = bq27505_write_multi_for_dfi(0x04, yIFRowData[iRow], 96, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 4 \n");
				goto FAILURE;
			}

			Checksum = 0;
			for (iCol = 0; iCol < 96; iCol++)
				Checksum += yIFRowData[iRow][iCol];
			Checksum = (0x02 + iRow + Checksum) & 0xFFFF;

			// LSB of Checksum
			err = bq27505_write_for_dfi(0x64, LOBYTE(Checksum), g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 5 \n");
				goto FAILURE;
			}
			// MSB of Checksum, Checksum is required to complete Write Row command
			err = bq27505_write_for_dfi(0x65, HIBYTE(Checksum), g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 6 \n");
				goto FAILURE;
			}

			mdelay(20);

			buf = 0xFF;
			err = bq27505_read_for_dfi(0x66, &buf, 1, g_di);
			if (err < 0)
			{
				dev_err(&g_di->client->dev, "error 3rd stage - 7 \n");
				goto FAILURE;
			}
			if (buf == 0)
				break;
		}

		if (buf != 0x0)
		{
			dev_err(&g_di->client->dev, "error 3rd stage - 8 \n");
			goto FAILURE;
		}
		dev_err(&g_di->client->dev, "3rd stage(1/2) complete (iRow = %d, retry cnt = %d) \n", iRow, iRetry);
	}

	// This command exits from ROM Mode
	err = bq27505_write_for_dfi(0x00, 0x0F, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 3rd stage - 9 \n");
		goto FAILURE;
	}

	err = bq27505_write_for_dfi(0x64, 0x0F, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 3rd stage - 10 \n");
		goto FAILURE;
	}

	err = bq27505_write_for_dfi(0x65, 0x00, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "error 3rd stage - 11 \n");
		goto FAILURE;
	}

	dev_err(&g_di->client->dev, "3rd stage(2/2) complete \n");

	mdelay(500);

	err = bq27505_write_sub_command(BQ27505_SUB_RESET, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "RESET error \n");
		goto FAILURE;
	}

	mdelay(500);

	err = bq27505_write_sub_command(BQ27505_SUB_IT_ENABLE, g_di);
	if (err < 0)
	{
		dev_err(&g_di->client->dev, "IT_ENABLE error \n");
		goto FAILURE;
	}

	dev_err(&g_di->client->dev, "4th stage complete(RESET, IT_ENABLE) \n");

	return ret;

FAILURE:

	mdelay(500);

	err = bq27505_write_sub_command(BQ27505_SUB_RESET, g_di);
	if (err < 0)
		dev_err(&g_di->client->dev, "RESET error \n");

	mdelay(500);

	err = bq27505_write_sub_command(BQ27505_SUB_IT_ENABLE, g_di);
	if (err < 0)
		dev_err(&g_di->client->dev, "IT_ENABLE error \n");

	return -1;
}

// return value 0 -> success
static int bq27505_update(void)
{
 	int cmd[11] = {
		BQ27505_STD_VOLT,
		BQ27505_STD_NAC,
		BQ27505_STD_FAC,
		BQ27505_STD_RM,
		BQ27505_STD_FCC,
		BQ27505_STD_AI,
		BQ27505_STD_TTE,
		BQ27505_STD_TTF,
		BQ27505_STD_SOH,
		BQ27505_STD_SOC,
		BQ27505_STD_NIC
	};
	int i, j, ret, data, err, status;
	unsigned char buf[2];
	struct bq27505_device_info *di = g_di;
	bool no_battery = false;

	di->is_updating_dfi = 1;

	bq27505_battery_interrupt(false);
	wake_lock(&di->wake_lock);

	dev_info(&di->client->dev, "%s \n", __func__);

	for (i = 0; i < 200; i++)
	{
		for (j = 0; j < sizeof(cmd) / sizeof(int); j++)
		{
			err = bq27505_read(cmd[j], buf, 2, di);
			if (err < 0)
			{
				dev_err(&di->client->dev, "Failed to read - cmd(0x%X) \n", cmd[j]);
				no_battery = true;
			}
			else
			{
				if (BQ27505_STD_AI == cmd[j])
				{
					data = (buf[1] << 8) | buf[0];
					dev_info(&di->client->dev, "cmd(0x%X) = %d \n", cmd[j], data);
					if (BQ27505_STD_AI == cmd[j] && 0 == data)
						no_battery = true;
				}
			}
			if (no_battery)
				break;
		}
		if (no_battery)
			break;

		err = bq27505_read_sub_command(BQ27505_SUB_CONTROL_STATUS, &status, di);
		if (err < 0)
			dev_err(&di->client->dev, "Failed to Read Control Status \n");
		else
		{
			dev_info(&di->client->dev, "control_status = 0x%04X \n", status);
			if (status && (status & 0x6C60) == 0)
			{
				dev_info(&di->client->dev, "Status cleared !!!\n");
				break;
			}

			if (status & 0x6000) // High Byte : BIT 6 - FAS, BIT 5 - SS
			{
				for (j = 0; j < 3; j++)
				{
					if (status & 0x2000) // SEALED state
					{
						// Set UNSEALED mode
						err = bq27505_write_sub_command(0x0414, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "Unseal Key 1 error \n");
							goto mode_retry;
						}
						err = bq27505_write_sub_command(0x3672, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "Unseal Key 0 error  \n");
							goto mode_retry;
						}
					}
					mdelay(25);

					if (status & 0x4000) // FULL ACCESS SEALED state
					{
						// Clear Full ACCESS SEALED mode
						err = bq27505_write_sub_command(0xFFFF, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "Full-Access Key 1 error \n");
							goto mode_retry;
						}
						err = bq27505_write_sub_command(0xFFFF, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "Full-Access Key 0 error \n");
							goto mode_retry;
						}
					}
					mdelay(25);
mode_retry:
					if (err == 0)
						break;
				}
			}
		}

		err = bq27505_read_sub_command(BQ27505_SUB_CONTROL_STATUS, &status, di);
		if (err < 0)
			dev_err(&di->client->dev, "Failed to Read Control Status \n");
		else
		{
			dev_info(&di->client->dev, "control_status = 0x%04X \n", status);
			if (status && (status & 0x6C60) == 0)
			{
				dev_info(&di->client->dev, "Status cleared !!!\n");
				break;
			}

			if (status & 0x0060) // Low Byte : BIT 6 - HIBERNATE, BIT 5 - SNOOZE
			{
				for (j = 0; j < 3; j++)
				{
					if (status & 0x0040)
					{
						err = bq27505_write_sub_command(BQ27505_SUB_DISABLE_SLEEP, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "DISABLE SLEEP error \n");
							goto mode_retry2;
						}
					}
					mdelay(25);

					if (status & 0x0020)
					{
						err = bq27505_write_sub_command(BQ27505_SUB_CLEAR_HIBERNATE, di);
						if (err < 0)
						{
							dev_err(&di->client->dev, "CLEAR HIBERNATE error \n");
							goto mode_retry2;
						}
					}
					mdelay(25);
mode_retry2:
					if (err == 0)
						break;
				}
			}
		}

		mdelay(100);
	}

	if ((status & 0x0010) || no_battery) // sleep state or no battery
	{
		ret = -1;
		dev_err(&di->client->dev, "%s - Sleep state or No Battery !!!\n", __func__);
		goto exit_update;
	}

	if (err == 0)
	{
		/* Update DFI Image */
		ret = bq27505_update_dfi();
	}
	else
		ret = -1;

exit_update:
	di->is_updating_dfi = 0;

	dev_info(&di->client->dev, "Exit Update Routine !!!\n");
	wake_unlock(&di->wake_lock);
	bq27505_battery_interrupt(true);

	return ret;
}

#ifdef BQ27505_PROC_TEST
static int bq27505_read_proc
	(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int rt_value, err;

	int i;
 	int cmd[11] = {
		BQ27505_STD_VOLT,
		BQ27505_STD_NAC,
		BQ27505_STD_FAC,
		BQ27505_STD_RM,
		BQ27505_STD_FCC,
		BQ27505_STD_AI,
		BQ27505_STD_TTE,
		BQ27505_STD_TTF,
		BQ27505_STD_SOH,
		BQ27505_STD_SOC,
		BQ27505_STD_NIC
	};

	unsigned char buf[100];

	for (i = 0; i < sizeof(cmd) / sizeof(int); i++)
	{
		err = bq27505_battery_property(cmd[i], &rt_value);
		dev_err(&g_di->client->dev, "cmd = 0x%02X, data = 0x%04X, %d \n",
			cmd[i], rt_value, rt_value);
	}

	rt_value = 0;
	err = bq27505_read_sub_command(BQ27505_SUB_CONTROL_STATUS, &rt_value, g_di);
	dev_err(&g_di->client->dev, "err = %d, CONTROL_STAUTS = 0x%X \n", err, rt_value);

	// Unseal Key 0 - 0x3672
	// Unseal Key 1 - 0x0414

	buf[0] = 0x14;
	buf[1] = 0x04;
	buf[2] = 0x72;
	buf[3] = 0x36;

	// Full-Access Key 0 - 0xFFFF
	// Full-Access Key 1 - 0xFFFF
	buf[4] = 0xFF;
	buf[5] = 0xFF;
	buf[6] = 0xFF;
	buf[7] = 0xFF;

	if (rt_value & 0x2000)
	{
		// UNSEALED mode
		err = bq27505_write(BQ27505_STD_CNTL, buf, 2, g_di);
		dev_err(&g_di->client->dev, "err = %d \n", err);

		err = bq27505_write(BQ27505_STD_CNTL, &buf[2], 2, g_di);
		dev_err(&g_di->client->dev, "err = %d \n", err);
	}

	if (rt_value & 0x4000)
	{
		// clear FULL-ACCESS mode
		err = bq27505_write(BQ27505_STD_CNTL, &buf[4], 2, g_di);
		dev_err(&g_di->client->dev, "err = %d \n", err);

		err = bq27505_write(BQ27505_STD_CNTL, &buf[6], 2, g_di);
		dev_err(&g_di->client->dev, "err = %d \n", err);
	}

	rt_value = 0;
	err = bq27505_read_sub_command(BQ27505_SUB_CONTROL_STATUS, &rt_value, g_di);
	dev_err(&g_di->client->dev, "err = %d, CONTROL_STAUTS = 0x%X \n", err, rt_value);

	rt_value = 0;
	err = bq27505_read_sub_command(BQ27505_SUB_DEVICE_TYPE, &rt_value, g_di);
	dev_err(&g_di->client->dev, "err = %d, DEVICE_TYPE = 0x%X \n", err, rt_value);
	rt_value = 0;
	err = bq27505_read_sub_command(BQ27505_SUB_FW_VERSION, &rt_value, g_di);
	dev_err(&g_di->client->dev, "err = %d, FW_VERSION = 0x%X \n", err, rt_value);
	rt_value = 0;
	err = bq27505_read_sub_command(BQ27505_SUB_HW_VERSION, &rt_value, g_di);
	dev_err(&g_di->client->dev, "err = %d, HW_VERSION = 0x%X \n", err, rt_value);

#if 0
/*
	rt_value = 0;
	err = bq27505_read(0x62, buf, 1, g_di);
	rt_value = buf[0];
	dev_err(&g_di->client->dev, "err = %d, device name length = %d \n", err, rt_value);
	for (i = 0x63; i < 0x6A; i++)
	{
		rt_value = 0;
		err = bq27505_read(i, buf, 1, g_di);
		dev_err(&g_di->client->dev, "err = %d, device name = %c \n", err, (char)rt_value);
	}
*/
	err = bq27505_read(0x62, buf, 1, g_di);
	rt_value = buf[0];
	dev_err(&g_di->client->dev, "err = %d, device name length = %d \n", err, rt_value);
	err = bq27505_read(0x63, buf, rt_value, g_di);
	printk(KERN_ERR "");
	for (i = 0; i < rt_value; i++)
	{
		printk("%c", buf[i]);
	}
	printk("\n");
#endif

	buf[0] = 0x0; // enable BlockData
	err = bq27505_write(0x61, buf, 1, g_di); // BlockDataControl
	if (err < 0)
		dev_err(&g_di->client->dev, "error %d", __LINE__);

//	buf[0] = 0x50; // Subclass ID - 80
	buf[0] = 112; // Subclass ID
	err = bq27505_write(0x3E, buf, 1, g_di);  // DataFlashClass
	if (err < 0)
		dev_err(&g_di->client->dev, "error %d", __LINE__);

//	buf[0] = 0x1; // 2nd 32bytes block, block offset
	buf[0] = 0x0; // 1st 32bytes block, block offset
	err = bq27505_write(0x3F, buf, 1, g_di);  // DataFlashBlock
	if (err < 0)
		dev_err(&g_di->client->dev, "error %d", __LINE__);

	memset(buf, 0x0, 100);
	err = bq27505_read(0x40, buf, 32, g_di); // BlockData
	printk(KERN_ERR "");
	for (i = 0; i < 32; i++)
	{
		if (i > 0 && ((i % 16) == 0))
		{
			printk("\n");
			printk(KERN_ERR "");
		}
		printk("0x%02X, ", buf[i]);
	}
	printk("\n");

	return 0;
}
#endif

#if 0//def CONFIG_HAS_EARLYSUSPEND
static void bq27505_early_suspend(struct early_suspend *h)
{
}

static void bq27505_late_resume(struct early_suspend *h)
{
	int err, soc;
	err = bq27505_battery_property(BQ27505_STD_SOC, &soc);
	printk(KERN_ERR "%s, soc = %d err = %d \n", __func__, soc, err);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static void bq27505_isr_work_func(struct work_struct *work)
{
	int soc, volt, cnt, err;
	struct bq27505_device_info *di
		= container_of(work, struct bq27505_device_info, isr_work);

	if (di->pwr_sts == 0)
	{
		printk(KERN_ERR "%s, Device is off state !!!\n", __func__);
		return;
	}

	bq27505_battery_interrupt(false);
	wake_lock(&di->wake_lock);

	cnt = 0;
	err = 0;
	while (cnt++ < 10)
	{
		soc = 0;
		err = bq27505_battery_property(BQ27505_STD_SOC, &soc);
		if (err || soc == 0)
		{
			printk(KERN_ERR "%s, soc = %d err = %d \n", __func__, soc, err);
			msleep(100);
			continue;
		}
		break;
	}
	printk(KERN_ERR "%s, soc = %d, cnt = %d \n", __func__, soc, cnt);

	if (err == 0 && soc == 0)
	{
		err = bq27505_battery_property(BQ27505_STD_VOLT, &volt);
		if (err == 0 && volt == 0)
		{
			printk(KERN_ERR "%s, volt = %d \n", __func__, volt);
			wake_unlock(&di->wake_lock);
			bq27505_battery_interrupt(true);
			return;
		}
	}

#ifdef CONFIG_MACH_QSD8X50_S1
	msm_batt_update_psy_status();
#endif

	wake_unlock(&di->wake_lock);
	bq27505_battery_interrupt(true);
}

#ifdef BQ27505_OLD_STYLE
static void bq27505_resume_work_func(struct work_struct *work)
{
//	struct bq27505_device_info *di
//		= container_of(work, struct bq27505_device_info, resume_work);

	printk(KERN_ERR "%s \n", __func__);

	wake_unlock(&g_di->wake_lock);

	bq27505_battery_interrupt(true);
}
#endif

static irqreturn_t bq27505_isr(int irq, void *handle)
{
	struct bq27505_device_info *di = handle;

	printk(KERN_ERR "%s\n", __func__);

	if (!bq27505_probed)
		return IRQ_HANDLED;

	schedule_work(&di->isr_work);

	return IRQ_HANDLED;
}


/*
 * Misc Device code for BQ27505 devices
 */

static int bq27505_battery_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int bq27505_battery_release(struct inode *inode, struct file *file)
{
	return 0;
}

#define BQ27505_IO_MAGIC          0x55
#define BQ27505_IOCTL_STD_CMD     _IOWR(BQ27505_IO_MAGIC, 0x0, int)
#define BQ27505_IOCTL_SUB_CMD     _IOWR(BQ27505_IO_MAGIC, 0x1, int)
#define BQ27505_IOCTL_UPDATE_CMD  _IOWR(BQ27505_IO_MAGIC, 0x2, int)

static int bq27505_battery_ioctl(struct inode *inode, struct file *file,
					unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	int err, reg, buffer;

	if (g_di->is_updating_dfi) // updating dfi is progressing.
		return -EFAULT;

	if (copy_from_user(&reg, argp, sizeof(reg)))
		return -EFAULT;

	err = 0;
	switch (cmd)
	{
	case BQ27505_IOCTL_STD_CMD:
		err = bq27505_battery_property(reg, &buffer);
		if (copy_to_user(argp, &buffer, sizeof(buffer)))
			return -EFAULT;
		break;
	case BQ27505_IOCTL_SUB_CMD:
		err = bq27505_read_sub_command(reg, &buffer, g_di);
		if (err < 0)
		{
			dev_err(&g_di->client->dev, "%s : sub cmd(%d) error(%d) \n",
				__func__, reg, err);
			break;
		}
		if (copy_to_user(argp, &buffer, sizeof(buffer)))
			return -EFAULT;
		break;
	case BQ27505_IOCTL_UPDATE_CMD:
		buffer = bq27505_update();
		err = buffer;
		if (copy_to_user(argp, &buffer, sizeof(buffer)))
			return -EFAULT;
		break;
	default:
		dev_err(&g_di->client->dev, "%s : Invalid cmd(0x%X)!!! \n", __func__, cmd);
		break;
	}

	return err;
}

static struct file_operations bq27505_fops = {
	.owner   = THIS_MODULE,
	.open    = bq27505_battery_open,
	.release = bq27505_battery_release,
	.ioctl   = bq27505_battery_ioctl,
};

static struct miscdevice bq27505_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "bq27505",
	.fops  = &bq27505_fops,
};


#ifdef CONFIG_PM
static int bq27505_pm_suspend(struct device *dev)
{
#ifdef BQ27505_OLD_STYLE
	bq27505_battery_interrupt(false);

	if (g_di->pwr_pin != -1)
	{
		g_di->pwr_sts = 0;
		gpio_set_value(g_di->pwr_pin, 0);
	}
#endif

	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static int bq27505_pm_resume(struct device *dev)
{
#ifdef BQ27505_OLD_STYLE
	if (g_di->pwr_pin != -1)
	{
		gpio_set_value(g_di->pwr_pin, 1);
		g_di->pwr_sts = 1;
	}

	wake_lock(&g_di->wake_lock);
	schedule_delayed_work(&g_di->resume_work, HZ);

	bq27505_battery_interrupt(true);
#endif

	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static struct dev_pm_ops bq27505_pm_ops = {
	.suspend = bq27505_pm_suspend,
	.resume  = bq27505_pm_resume,
};
#endif


/*
 * I2C Device code for BQ27505 devices
 */

static int bq27505_battery_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	unsigned char buf[2];
	struct bq27505_device_info *di;
	struct bq27505_platform_data *pdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk(KERN_ERR "bq27505 failed to check i2c communication!\n");
		goto err_check_functionality_failed;
	}

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	if (pdata->init_gpio_cfg)
	{
		err = pdata->init_gpio_cfg();
		if (err)
			return -EINVAL;
	}

	if (pdata->pwr_pin != -1 && gpio_get_value(pdata->pwr_pin) == 0)
	{
		gpio_set_value(pdata->pwr_pin, 1);
		mdelay(500);
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		return -ENOMEM;
	}

	INIT_WORK(&di->isr_work, bq27505_isr_work_func);
#ifdef BQ27505_OLD_STYLE
	INIT_DELAYED_WORK(&di->resume_work, bq27505_resume_work_func);
#endif

	i2c_set_clientdata(client, di);
	di->dev             = &client->dev;
	di->client          = client;
	di->pwr_pin         = pdata->pwr_pin;
	di->pwr_sts         = 1;
	di->irq             = client->irq;
	di->irq_enabled     = true;
	di->is_updating_dfi = 0;
	g_di = di;

	err = bq27505_read(BQ27505_STD_SOC, buf, 2, di);
	if (err) {
		dev_err(&client->dev, "%s - error reading property(cmd:0x%X) = %d \n",
			__func__, BQ27505_STD_SOC, err);
		goto err_free_mem;
	}

	err = request_irq(di->irq, bq27505_isr, IRQF_TRIGGER_FALLING,
			client->dev.driver->name, di);
	if (err < 0) {
		dev_err(&client->dev, "irq %d error \n", di->irq);
		goto err_free_mem;
	}
	set_irq_wake(di->irq, 1);

	err = misc_register(&bq27505_device);
	if (err) {
		dev_err(&client->dev, "%s: failed to register misc device\n", __func__);
		goto err_free_irq;
	}

	wake_lock_init(&di->wake_lock, WAKE_LOCK_SUSPEND, "bq27505_gauge");

#if 0//def CONFIG_HAS_EARLYSUSPEND
	di->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	di->early_suspend.suspend = bq27505_early_suspend;
	di->early_suspend.resume = bq27505_late_resume;
	register_early_suspend(&di->early_suspend);
#endif

	dev_info(&client->dev, "probe ok \n");

	bq27505_probed = true;

	return 0;

err_free_irq:
	free_irq(di->irq, di);

err_free_mem:
	kfree(di);

err_check_functionality_failed:
	return err;
}

static int bq27505_battery_remove(struct i2c_client *client)
{
	struct bq27505_device_info *di = i2c_get_clientdata(client);

	wake_lock_destroy(&di->wake_lock);

#if 0//def CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&di->early_suspend);
#endif
	free_irq(di->irq, di);

	kfree(di);

	g_di = NULL;

	return 0;
}


/*
 * Module stuff
 */

static const struct i2c_device_id bq27505_id[] = {
	{ "bq27505", 0 },
	{},
};

static struct i2c_driver bq27505_battery_driver = {
	.driver = {
		.name  = "bq27505",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm    = &bq27505_pm_ops,
#endif
	},
	.probe = bq27505_battery_probe,
	.remove = bq27505_battery_remove,
	.id_table = bq27505_id,
};

static int __init bq27505_battery_init(void)
{
	int ret;
#ifdef BQ27505_PROC_TEST
	struct proc_dir_entry *d_entry;
#endif

	ret = i2c_add_driver(&bq27505_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27200 driver\n");

#ifdef BQ27505_PROC_TEST
	d_entry = create_proc_entry("bq27505",
			S_IRUGO | S_IWUSR | S_IWGRP, NULL);
	if (d_entry) {
		d_entry->read_proc = bq27505_read_proc;
		d_entry->write_proc = NULL;
		d_entry->data = NULL;
	}
#endif

	return ret;
}
module_init(bq27505_battery_init);

static void __exit bq27505_battery_exit(void)
{
	i2c_del_driver(&bq27505_battery_driver);
}
module_exit(bq27505_battery_exit);

MODULE_AUTHOR("Daehee Kim <daehee.kim@sk-w.com>");
MODULE_DESCRIPTION("BQ27505 fuel gauge driver");
MODULE_LICENSE("GPL");
