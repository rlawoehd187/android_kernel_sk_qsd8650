/* linux/arch/arm/mach-msm/board-s1-rfkill.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

/* Control bluetooth power for s1 platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <linux/gpio.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>

static struct rfkill * s1_rfkill_register(struct device *dev,
                         enum rfkill_type type, char *name, struct rfkill_ops *ops)
{
	int err;
	struct rfkill *rfkill_dev;
	bool default_state = true;  /* off */

	rfkill_dev = rfkill_alloc(name, dev, type, ops, NULL);
	
	if (!rfkill_dev)
		return ERR_PTR(-ENOMEM);

	rfkill_set_states(rfkill_dev, default_state, false);
	err = rfkill_register(rfkill_dev);
	if (err) {
		rfkill_destroy(rfkill_dev);
		return ERR_PTR(err);
	}
	return rfkill_dev;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
static uint32_t s1_wifi_io_sdio_power(int on)
#else
static uint32_t s1_bt_wifi_io_sdio_power(int on)
#endif
{
	int rc = 0;
	struct vreg *vreg_bt_wifi_io_sdio;

	vreg_bt_wifi_io_sdio = vreg_get(NULL, S1_PM_WIFI_SDIO_NAME);
	if (IS_ERR(vreg_bt_wifi_io_sdio)) {
		printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
			__func__, S1_PM_WIFI_SDIO_NAME, PTR_ERR(vreg_bt_wifi_io_sdio));
		return -EIO;
	}

	if (on)
	{
		rc = vreg_set_level(vreg_bt_wifi_io_sdio, S1_PM_WIFI_SDIO_LEVEL);
		if (rc) {
			printk(KERN_ERR "%s: vreg " S1_PM_WIFI_SDIO_NAME "set level failed (%d)\n",
				__func__, rc);
		}

		rc = vreg_enable(vreg_bt_wifi_io_sdio);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable failed (%d)\n",
				 __func__, rc);
		}
	}
	else
	{
		rc = vreg_set_level(vreg_bt_wifi_io_sdio, 0);
		if (rc) {
			printk(KERN_ERR "%s: vreg " S1_PM_WIFI_SDIO_NAME "set level failed (%d)\n",
				__func__, rc);
		}
		rc = vreg_disable(vreg_bt_wifi_io_sdio);
		if (rc) {
			printk(KERN_ERR "%s: vreg disable failed (%d)\n",
				 __func__, rc);
		}
	}
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
	mdelay(50);
#endif
	return 0;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
static int s1_bt_io_power(int on)
{
	int rc;
	struct vreg *vreg_bt_vddio;

	vreg_bt_vddio = vreg_get(NULL, S1_PM_BT_NAME);

	if (IS_ERR(vreg_bt_vddio)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_bt_vddio));
		return PTR_ERR(vreg_bt_vddio);
	}
	
	if(on)
	{
		/* units of mV, steps of 50 mV */
		rc = vreg_set_level(vreg_bt_vddio, S1_PM_BT_LEVEL);
		if (rc) {
			printk(KERN_ERR "%s: vreg bt set level failed (%d)\n",
			__func__, rc);
			return -EIO;
		}
		rc = vreg_enable(vreg_bt_vddio);
		if (rc) {
			printk(KERN_ERR "%s: vreg bt enable failed (%d)\n",
			__func__, rc);
			return -EIO;
		}
	}
	else
	{
		rc = vreg_set_level(vreg_bt_vddio, 0);
		if (rc) {
		printk(KERN_ERR "%s: vreg bt set level failed (%d)\n",
			__func__, rc);
			return -EIO;
		}
		rc = vreg_disable(vreg_bt_vddio);
		if (rc) {
			printk(KERN_ERR "%s: bt wlan disable failed (%d)\n",
			__func__, rc);
			return -EIO;
		}
	}
	mdelay(50);
	
	return 0;
}

#else
static long unsigned int bt_wifi_pwr_sts = 0;

static int wifi_bt_clk_power(int dev_id, int on)
{
	int rc;
	struct vreg *vreg_bt_wifi_clk;

	vreg_bt_wifi_clk = vreg_get(NULL, S1_PM_WIFI_BT_CLK_NAME);

	if (IS_ERR(vreg_bt_wifi_clk)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_bt_wifi_clk));
		return PTR_ERR(vreg_bt_wifi_clk);
	}

	printk(KERN_DEBUG ">>> 1. WIFI-BT combo clock power switch: %ld\n", bt_wifi_pwr_sts);
	if (on) 
	{
		if (bt_wifi_pwr_sts == 0)
		{
			/* units of mV, steps of 50 mV */
			rc = vreg_set_level(vreg_bt_wifi_clk, S1_PM_WIFI_BT_CLK_LEVEL);
			if (rc) {
				printk(KERN_ERR "%s: vreg wlan set level failed (%d)\n",
				       __func__, rc);
				return -EIO;
			}
			rc = vreg_enable(vreg_bt_wifi_clk);
			if (rc) {
				printk(KERN_ERR "%s: vreg wlan enable failed (%d)\n",
				       __func__, rc);
				return -EIO;
			}
			mdelay(100);
			s1_bt_wifi_io_sdio_power(1);
		}
		set_bit(dev_id, &bt_wifi_pwr_sts);
	}
	else
	{
		if (bt_wifi_pwr_sts != 0)
		{
			clear_bit(dev_id, &bt_wifi_pwr_sts);
			if (bt_wifi_pwr_sts == 0)
			{
				rc = vreg_set_level(vreg_bt_wifi_clk, 0);
				if (rc) {
					printk(KERN_ERR "%s: vreg wlan set level failed (%d)\n",
					       __func__, rc);
					return -EIO;
				}
				rc = vreg_disable(vreg_bt_wifi_clk);
				if (rc) {
					printk(KERN_ERR "%s: vreg wlan disable failed (%d)\n",
							__func__, rc);
					return -EIO;
				}
				mdelay(100);
				s1_bt_wifi_io_sdio_power(0);
			}
		}
	}
	printk(KERN_DEBUG ">>> 2. WIFI-BT combo clock power switch: %ld\n", bt_wifi_pwr_sts);
	
	return 0;
}
#endif

/*
***************************************************************************************************
* WIFI & RF Kill
*
***************************************************************************************************
*/
static struct msm_gpio	wifi_config_power_off[] = {
	{ GPIO_CFG(S1_GPIO_WIFI_CLK, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_CLK" },
	{ GPIO_CFG(S1_GPIO_WIFI_CMD, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_CMD" },
	{ GPIO_CFG(S1_GPIO_WIFI_D3, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_D3" },
	{ GPIO_CFG(S1_GPIO_WIFI_D2, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_D2" },
	{ GPIO_CFG(S1_GPIO_WIFI_D1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_D1" },
	{ GPIO_CFG(S1_GPIO_WIFI_D0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"SDC2_D0" },
	{ GPIO_CFG(S1_GPIO_WIFI_REG_ON, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"VDD_WLAN" },
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
	{ GPIO_CFG(S1_GPIO_WIFI_RST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"WLAN_PWD" },
#endif		
	{ GPIO_CFG(S1_GPIO_WIFI_HOST_WAKE, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"WLAN_H_WAKE" },
	{ GPIO_CFG(S1_GPIO_WIFI_WAKE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"WLAN_WAKE" },
};

static struct msm_gpio wifi_config_power_on[] = {
	{ GPIO_CFG(S1_GPIO_WIFI_CLK, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		"SDC2_CLK" },
	{ GPIO_CFG(S1_GPIO_WIFI_CMD, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		"SDC2_CMD" },
	{ GPIO_CFG(S1_GPIO_WIFI_D3, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		"SDC2_D3" },
	{ GPIO_CFG(S1_GPIO_WIFI_D2, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		"SDC2_D2" },
	{ GPIO_CFG(S1_GPIO_WIFI_D1, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		"SDC2_D1" },
	{ GPIO_CFG(S1_GPIO_WIFI_D0, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		"SDC2_D0" },
	{ GPIO_CFG(S1_GPIO_WIFI_REG_ON, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"VDD_WLAN" },
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
	{ GPIO_CFG(S1_GPIO_WIFI_RST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"WLAN_PWD" },
#endif		
	{ GPIO_CFG(S1_GPIO_WIFI_HOST_WAKE, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
		"WLAN_H_WAKE" },
	{ GPIO_CFG(S1_GPIO_WIFI_WAKE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"WLAN_WAKE" },
};

int s1_wifi_power(int on)
{
	int	rc;

	if (on) {
		rc = msm_gpios_enable(wifi_config_power_on,
					ARRAY_SIZE(wifi_config_power_on));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: wifi power on gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
		s1_wifi_io_sdio_power(1);
#else
		wifi_bt_clk_power(S1_PM_WIFI_CLK_BIT, 1);
#endif

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
		gpio_set_value(S1_GPIO_WIFI_RST, 0); 		/* SYSRST */
		mdelay(50);
		gpio_set_value(S1_GPIO_WIFI_RST, 1); 		/* SYSRST */
#endif
		gpio_set_value(S1_GPIO_WIFI_REG_ON, 1);
		gpio_set_value(S1_GPIO_WIFI_WAKE, 1);
	}
	else 
	{
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
		gpio_set_value(S1_GPIO_WIFI_RST, 0); 		/* SYSRST */
#endif
		gpio_set_value(S1_GPIO_WIFI_REG_ON, 0);
		gpio_set_value(S1_GPIO_WIFI_WAKE, 0);
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
		s1_wifi_io_sdio_power(0);
#else
		wifi_bt_clk_power(S1_PM_WIFI_CLK_BIT, 0);
#endif		
		rc = msm_gpios_enable(wifi_config_power_off,
					ARRAY_SIZE(wifi_config_power_off));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: wifi power off gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}
	}
	mdelay(100); 
	printk(KERN_DEBUG "WIFI power switch: %d\n", on);

	return 0;
}

#if 0
void bcm_wlan_power_off(int state)
{
	if (state == 1)
	{
		s1_wifi_power(0);
	}
	else if (state == 2)
	{
		gpio_set_value(S1_GPIO_WIFI_RST, 0); 		/* SYSRST */
	}
}

void bcm_wlan_power_on(int state)
{
	if (state == 1)
	{
		s1_wifi_power(1);
	}
	else if (state == 2)
	{
		gpio_set_value(S1_GPIO_WIFI_RST, 1); 		/* SYSRST */
	}
}
#endif

static struct rfkill        	*g_RFKillWifi;

static int s1_wifi_rfkill_set(void *data, bool state)
{
	printk(KERN_ERR ">>> wifi_rfkill_set : %d\n", state);

	if (state == 1)        /* ON */
	{
		s1_wifi_power(1);
	}
	else
	{
		s1_wifi_power(0);
	}

	return 0;
}

static int s1_wifi_rfkill_get_state(void *data, bool *state)
{
	*state = gpio_get_value(S1_GPIO_WIFI_REG_ON) ? 1 : 0;
	printk(KERN_ERR ">>> s1_wifi_rfkill_get_state : %d\n", *state);
	return 0;
}

static struct rfkill_ops wifi_rfkill_ops = {
	.set_block = s1_wifi_rfkill_set,
};

static int s1_wifi_rfkill_init(struct device *dev)
{
	g_RFKillWifi = s1_rfkill_register(dev, RFKILL_TYPE_WLAN,
                                            "s1-wifi", &wifi_rfkill_ops);
	if (IS_ERR(g_RFKillWifi)) {
		rfkill_unregister(g_RFKillWifi);
		return PTR_ERR(g_RFKillWifi);
	}

	return 0;
}


static void s1_wifi_rfkill_exit(void)
{
	if (g_RFKillWifi)
	{
		rfkill_unregister(g_RFKillWifi);
	}

	return;
}

/*
***************************************************************************************************
* BT & RF Kill
*
***************************************************************************************************
*/
static struct rfkill        *g_RFKillBT;

static struct msm_gpio bt_config_power_off[] = {
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
	{ GPIO_CFG(S1_GPIO_BT_RST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT SYSRST" },
#endif
	{ GPIO_CFG(S1_GPIO_BT_WAKE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT WAKE" },
	{ GPIO_CFG(S1_GPIO_BT_HOST_WAKE, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"HOST WAKE" },
	{ GPIO_CFG(S1_GPIO_BT_REG_ON, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT VDD_IO" },
	{ GPIO_CFG(S1_GPIO_BT_UART_RTS, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"UART1DM_RFR" },
	{ GPIO_CFG(S1_GPIO_BT_UART_CTS, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"UART1DM_CTS" },
	{ GPIO_CFG(S1_GPIO_BT_UART_RXD, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"UART1DM_RX" },
	{ GPIO_CFG(S1_GPIO_BT_UART_TXD, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"UART1DM_TX" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_DOUT, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_DOUT" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_DIN, 0, GPIO_INPUT,  GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_DIN" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_SYNC, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_SYNC" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_CLK, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_CLK" },
};

static struct msm_gpio bt_config_power_on[] = {
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
	{ GPIO_CFG(S1_GPIO_BT_RST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT SYSRST" },
#endif
	{ GPIO_CFG(S1_GPIO_BT_WAKE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT WAKE" },
	{ GPIO_CFG(S1_GPIO_BT_HOST_WAKE, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
		"HOST WAKE" },
	{ GPIO_CFG(S1_GPIO_BT_REG_ON, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"BT VDD_IO" },
	{ GPIO_CFG(S1_GPIO_BT_UART_RTS, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"UART1DM_RFR" },
	{ GPIO_CFG(S1_GPIO_BT_UART_CTS, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
		"UART1DM_CTS" },
	{ GPIO_CFG(S1_GPIO_BT_UART_RXD, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
		"UART1DM_RX" },
	{ GPIO_CFG(S1_GPIO_BT_UART_TXD, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"UART1DM_TX" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_DOUT, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		"AUX_PCM_DOUT" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_DIN, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
		"AUX_PCM_DIN" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_SYNC, 2, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_SYNC" },
	{ GPIO_CFG(S1_GPIO_AUX_PCM_CLK, 2, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
		"AUX_PCM_CLK" },
};


static int s1_bt_power(int on)
{
	int	rc;

	if (on) {
		rc = msm_gpios_enable(bt_config_power_on,
					ARRAY_SIZE(bt_config_power_on));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: bt power on gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
		s1_bt_io_power(1);
#else
		wifi_bt_clk_power(S1_PM_BT_CLK_BIT, 1);
#endif		

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
		gpio_set_value(S1_GPIO_BT_RST, 0);	/* SYSRST */
		mdelay(50);
		gpio_set_value(S1_GPIO_BT_RST, 1);	/* SYSRST */
#endif
		gpio_set_value(S1_GPIO_BT_REG_ON, 1);
		gpio_set_value(S1_GPIO_BT_WAKE, 1);

//		gpio_set_value(S1_GPIO_WIFI_REG_ON, 1);

		printk(KERN_DEBUG "!!!!!!!!!!!!!!!!!!!!!222\n");
	}
	else 
	{
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT01)
		gpio_set_value(S1_GPIO_BT_RST, 0);	/* SYSRST */
#endif
		gpio_set_value(S1_GPIO_BT_REG_ON, 0);
		gpio_set_value(S1_GPIO_BT_WAKE, 0);
		
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
		s1_bt_io_power(0);
#else	
		wifi_bt_clk_power(S1_PM_BT_CLK_BIT, 0);
#endif		
		rc = msm_gpios_enable(bt_config_power_off,
					ARRAY_SIZE(bt_config_power_off));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: bt power off gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}
	}
	mdelay(200); //100->200
	printk(KERN_DEBUG "Bluetooth power switch: %d\n", on);

	return 0;
}

static int s1_bt_rfkill_set(void *data, bool state)
{
	printk(KERN_ERR ">>> bt_rfkill_set : %d\n", state);

	if (state)
	{
		s1_bt_power(0);
	}
	else
	{
		s1_bt_power(1);
	}
	return 0;
}

static int s1_bt_rfkill_get_state(void *data, bool *state)
{
	*state = gpio_get_value(S1_GPIO_BT_REG_ON) ? 1 : 0;
	printk(KERN_ERR ">>> s1_bt_rfkill_get_state : %d\n", *state);
	return 0;
}

static struct rfkill_ops bt_rfkill_ops = {
	.set_block = s1_bt_rfkill_set,
};

static int s1_bt_rfkill_init(struct device *dev)
{
	g_RFKillBT = s1_rfkill_register(dev, RFKILL_TYPE_BLUETOOTH,
                                    "s1-bluetooth", &bt_rfkill_ops);
	if (IS_ERR(g_RFKillBT)) {
		rfkill_unregister(g_RFKillBT);
		return PTR_ERR(g_RFKillBT);
	}

	return 0;
}

static void s1_bt_rfkill_exit(void)
{
	if (g_RFKillBT)
	{
		rfkill_unregister(g_RFKillBT);
	}

	return;
}


/*
***************************************************************************************************
* BT & RF Kill
*
***************************************************************************************************
*/
static int s1_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = s1_bt_rfkill_init(&pdev->dev);
	if (rc)
	{
		goto exit;
	}

	rc = s1_wifi_rfkill_init(&pdev->dev);
	if (rc)
	{
		goto bt_exit;
	}

	return (rc);

bt_exit:
	s1_bt_rfkill_exit();
exit:
	return rc;
}


static int s1_rfkill_remove(struct platform_device *dev)
{
	s1_bt_rfkill_exit();
	s1_wifi_rfkill_exit();
	return 0;
}


static struct platform_driver s1_rfkill_driver = {
	.probe = s1_rfkill_probe,
	.remove = s1_rfkill_remove,
	.driver = {
		.name = "s1_rfkill",
		.owner = THIS_MODULE,
	},
};


static int __init s1_rfkill_init(void)
{
	return platform_driver_register(&s1_rfkill_driver);
}


static void __exit s1_rfkill_exit(void)
{
	platform_driver_unregister(&s1_rfkill_driver);
}

module_init(s1_rfkill_init);
module_exit(s1_rfkill_exit);
MODULE_DESCRIPTION("s1 rfkill");
MODULE_AUTHOR("TJ Kim <tjkim@sk-w.com>");
MODULE_LICENSE("GPL");
