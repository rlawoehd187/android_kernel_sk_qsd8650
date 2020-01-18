/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/mfd/tps65023.h>
#include <linux/bma150.h>
#include <linux/power_supply.h>
#include <linux/clk.h>
#include <linux/i2c-gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/io.h>
#include <asm/setup.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/sirc.h>
#include <mach/dma.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_hsusb_hw.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_touchpad.h>
#include <mach/msm_i2ckbd.h>
#include <mach/pmic.h>
#include <mach/camera.h>
#include <mach/memory.h>
#include <mach/msm_spi.h>
#include <mach/msm_tsif.h>
#include <mach/msm_battery.h>
#include <mach/rpc_server_handset.h>

#include "devices.h"
#include "timer.h"
#include "socinfo.h"
#include "msm-keypad-devices.h"
#include "pm.h"
#include "proc_comm.h"
#include "smd_private.h"
#include <mach/board-s1.h>
#include <linux/msm_kgsl.h>
#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android.h>
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#include <linux/i2c/tma340.h>
#else
#include <linux/i2c/tmg200.h>
#endif
#ifdef CONFIG_BATTERY_BQ27505
#include <linux/i2c/bq27505_battery.h>
#endif

#define SMEM_SPINLOCK_I2C	"S:6"
#define MSM_PMEM_SF_SIZE	0x02000000	// 32MB
#define MSM_PMEM_ADSP_SIZE	0x02196000
#define MSM_AUDIO_SIZE		0x00080000

#ifdef CONFIG_MSM_SOC_REV_A
#define MSM_SMI_BASE		0xE0000000
#else
#define MSM_SMI_BASE		0x00000000
#define MSM_SMI_END		0x04000000
#endif

#define MSM_SHARED_RAM_PHYS	(MSM_SMI_BASE + 0x00100000)			// 0x00100000

#define MSM_PMEM_SMI_BASE	(MSM_SMI_BASE + 0x02B00000)			// 0x02B00000
#define MSM_PMEM_SMI_SIZE	0x01500000					// 21MB

#define MSM_FB_BASE		MSM_PMEM_SMI_BASE				// 0x02B00000
#define MSM_FB_SIZE		0x002EE000 // (800 * 480 * 4) * 2

#define MSM_GPU_PHYS_BASE 	(MSM_FB_BASE + MSM_FB_SIZE)			// 0x02E00000
#define MSM_GPU_PHYS_SIZE 	0x00500000

//#define USE_SMI_POOL_EBI

#define MSM_PMEM_SMIPOOL_BASE	(MSM_GPU_PHYS_BASE + MSM_GPU_PHYS_SIZE)		// 0x03000000
#define MSM_PMEM_SMIPOOL_SIZE	(MSM_PMEM_SMI_SIZE - MSM_FB_SIZE \
					- MSM_GPU_PHYS_SIZE)			// 0x01000000 => 16M

#define PMEM_KERNEL_EBI1_SIZE	0x00028000
#define PMIC_VREG_GP6_LEVEL	2900

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
void msm_init_gpio_vibrator(void);
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
void msm_init_pmic_vibrator(void);
#endif

/*
***************************************************************************************************
*    USB
***************************************************************************************************
*/
/*
 * Windows XP Service Pack 2 OS can't recognize RNDIS+ADB USB configuration.
 * (2010-10-03 ykjeon@sk-w.com)
 */
#define FEATURE_SKTS_USB_RNDIS_XP_SP2_BUG_AVOID

#ifdef CONFIG_USB_FUNCTION
static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
	.nluns          = 0x02,
	.buf_size       = 16384,
	.vendor         = "GOOGLE",
	.product        = "Mass storage",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &usb_mass_storage_pdata,
	},
};
#endif

#ifdef CONFIG_USB_ANDROID
/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		/* MSC */
		.product_id         = 0xF000,
		.functions	    = 0x02,
		.adb_product_id     = 0x9015,
		.adb_functions	    = 0x12
	},
#ifdef CONFIG_USB_F_SERIAL
	{
		/* MODEM */
		.product_id         = 0xF00B,
		.functions	    = 0x06,
		.adb_product_id     = 0x901E,
		.adb_functions	    = 0x16,
	},
#endif
#ifdef CONFIG_USB_ANDROID_DIAG
	{
		/* DIAG */
		.product_id         = 0x900E,
		.functions	    = 0x04,
		.adb_product_id     = 0x901D,
		.adb_functions	    = 0x14,
	},
#endif
#if defined(CONFIG_USB_ANDROID_DIAG) && defined(CONFIG_USB_F_SERIAL)
	{
		/* DIAG + MODEM */
		.product_id         = 0x9004,
		.functions	    = 0x64,
		.adb_product_id     = 0x901F,
		.adb_functions	    = 0x0614,
	},
	{
		/* DIAG + MODEM + NMEA*/
		.product_id         = 0x9016,
		.functions	    = 0x764,
		.adb_product_id     = 0x9020,
		.adb_functions	    = 0x7614,
	},
	{
		/* DIAG + MODEM + NMEA + MSC */
		.product_id         = 0x9017,
		.functions	    = 0x2764,
		.adb_product_id     = 0x9018,
		.adb_functions	    = 0x27614,
	},
#endif
#ifdef CONFIG_USB_ANDROID_CDC_ECM
	{
		/* MSC + CDC-ECM */
		.product_id         = 0x9014,
		.functions	    = 0x82,
		.adb_product_id     = 0x9023,
		.adb_functions	    = 0x812,
	},
#endif
#ifdef CONFIG_USB_ANDROID_RMNET
	{
		/* DIAG + RMNET */
		.product_id         = 0x9021,
		.functions	    = 0x94,
		.adb_product_id     = 0x9022,
		.adb_functions	    = 0x914,
	},
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	{
		/* RNDIS */
		.product_id         = 0xF00E,
		.functions	    = 0xA,
		.adb_product_id     = 0x9024,
		.adb_functions	    = 0x1A,
	},
#endif
	/*
	 * Below things are for SK-S100 of SK telesys. ykjeon@sk-w.com
	 */
#if 0
	/* Detailed USB composition set */
	{
		/* MSC */
		.product_id         = 0xA001,
		.functions	    = 0x02,
		.adb_product_id     = 0xA002,
		.adb_functions	    = 0x12,
	},
#ifdef CONFIG_USB_ANDROID_EEM
	{
		/* DIAG + ACM MODEM + EEM + MSC */
		.product_id         = 0xA003,
		.functions	    = 0x2F34,
		.adb_product_id     = 0xA004,
		.adb_functions	    = 0x2F314,
	},
#endif /* CONFIG_USB_ANDROID_EEM */
#else
	/* Compact USB composition set */
	{
		
		.product_id         = 0xA001,	/* MSC */
		.functions	    = 0x02,
		.adb_product_id     = 0xA004,
		.adb_functions	    = 0x2F314,	/* DIAG + ADB + ACM MODEM + EEM + MSC */
	},
	{
		/* For Recovery Image */
		.product_id         = 0xA003,
		.functions	    = 0x2F34,
		.adb_product_id     = 0xA003,
		.adb_functions	    = 0x2F34,
	},
#endif /* 0 */
#ifdef CONFIG_USB_ANDROID_RNDIS
	{
		/* RNDIS */
		.product_id         = 0xA007,
		.functions	    = 0xA,	/* RNDIS */
#ifdef FEATURE_SKTS_USB_RNDIS_XP_SP2_BUG_AVOID /* 2010-10-03 ykjeon@sk-w.com */
		.adb_product_id     = 0xA007,
		.adb_functions	    = 0xA,	/* RNDIS */
#else
		.adb_product_id     = 0xA008,
		.adb_functions	    = 0x1A,	/* RNDIS + ADB */
#endif
	},
#endif
};

/* USB Vendor ID selection *//* 2010-05-05 ykjeon@sk-w.com */
#define QCT_VID		0x05C6
#define SKTS_VID	0x1F53
#define USB_VID		SKTS_VID

/* 2010-10-12 ykjeon@sk-w.com */
#define UMS_VENDOR_NAME			"SK      "			/* 8 bytes limit */
#define UMS_PRODUCT_NAME		"telesys SK-S100 "	/* 16 bytes limit */
#define UMS_RELEASE_NUMBER		0x0100

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
#if (USB_VID == SKTS_VID)
	.vendor		= UMS_VENDOR_NAME,
	.product	= UMS_PRODUCT_NAME,
	.release	= UMS_RELEASE_NUMBER,
#else
	.vendor		= "GOOGLE",
	.product	= "Mass Storage",
	.release	= 0xFFFF,
#endif
};
static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &mass_storage_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= USB_VID,
	.version	= 0x0100,
	.compositions   = usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
#if (USB_VID == SKTS_VID) /* 2010-05-05 ykjeon@sk-w.com */
	.product_name	= "SK telesys Android USB Device",
	.manufacturer_name = "SK telesys Incorporated",
#else
	.product_name	= "Qualcomm HSUSB Device",
	.manufacturer_name = "Qualcomm Incorporated",
#endif
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};
#endif

#ifdef CONFIG_USB_FUNCTION
static struct usb_function_map usb_functions_map[] = {
	{"diag", 0},
	{"adb", 1},
	{"modem", 2},
	{"nmea", 3},
	{"mass_storage", 4},
	{"ethernet", 5},
};

/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5, /* 0101 */
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15, /* 10101 */
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30, /* 110000 */
	},

	{
		.product_id         = 0x9015,
		.functions	    = 0x12, /* 10010 */
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD, /* 01101 */
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D, /* 11101 */
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10, /* 10000 */
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20, /* 100000 */
	},

	{
		.product_id         = 0x9018,
		.functions	    = 0x1F, /* 011111 */
	},

	{
		.product_id         = 0x901A,
		.functions	    = 0x0F, /* 01111 */
	},
};
#endif

static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "8k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

#ifdef CONFIG_USB_FS_HOST
static struct msm_gpio fsusb_config[] = {
	{ GPIO_CFG(139, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), "fs_dat" },
	{ GPIO_CFG(140, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), "fs_se0" },
	{ GPIO_CFG(141, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), "fs_oe_n" },
};

static int fsusb_gpio_init(void)
{
	return msm_gpios_request(fsusb_config, ARRAY_SIZE(fsusb_config));
}

static void msm_fsusb_setup_gpio(unsigned int enable)
{
	if (enable)
		msm_gpios_enable(fsusb_config, ARRAY_SIZE(fsusb_config));
	else
		msm_gpios_disable(fsusb_config, ARRAY_SIZE(fsusb_config));

}
#endif

#define MSM_USB_BASE              ((unsigned)addr)

static struct msm_hsusb_platform_data msm_hsusb_pdata = {
#ifdef CONFIG_USB_FUNCTION
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
	.vendor_id          = 0x5c6,
	.product_name       = "Qualcomm HSUSB Device",
	.serial_number      = "1234567890ABCDEF",
	.manufacturer_name  = "Qualcomm Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,

#endif
};

static struct vreg *vreg_usb;
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{

	switch (PHY_TYPE(phy_info)) {
	case USB_PHY_INTEGRATED:
		if (on)
			msm_hsusb_vbus_powerup();
		else
			msm_hsusb_vbus_shutdown();
		break;
	case USB_PHY_SERIAL_PMIC:
		if (on)
			vreg_enable(vreg_usb);
		else
			vreg_disable(vreg_usb);
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						phy_info);
	}

}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
	.vbus_power = msm_hsusb_vbus_power,
};

#ifdef CONFIG_USB_FS_HOST
static struct msm_usb_host_platform_data msm_usb_host2_pdata = {
	.phy_info	= USB_PHY_SERIAL_PMIC,
	.config_gpio = msm_fsusb_setup_gpio,
	.vbus_power = msm_hsusb_vbus_power,
};
#endif

static struct android_pmem_platform_data android_pmem_kernel_ebi1_pdata = {
	.name = PMEM_KERNEL_EBI1_DATA_NAME,
	/* if no allocator_type, defaults to PMEM_ALLOCATORTYPE_BITMAP,
	 * the only valid choice at this time. The board structure is
	 * set to all zeros by the C runtime initialization and that is now
	 * the enum value of PMEM_ALLOCATORTYPE_BITMAP, now forced to 0 in
	 * include/linux/android_pmem.h.
	 */
	.cached = 0,
};

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION

static struct android_pmem_platform_data android_pmem_kernel_smi_pdata = {
	.name = PMEM_KERNEL_SMI_DATA_NAME,
	/* if no allocator_type, defaults to PMEM_ALLOCATORTYPE_BITMAP,
	 * the only valid choice at this time. The board structure is
	 * set to all zeros by the C runtime initialization and that is now
	 * the enum value of PMEM_ALLOCATORTYPE_BITMAP, now forced to 0 in
	 * include/linux/android_pmem.h.
	 */
	.cached = 0,
};

#endif

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_smipool_pdata = {
	.name = "pmem_smipool",
	.start = MSM_PMEM_SMIPOOL_BASE,
	.size = MSM_PMEM_SMIPOOL_SIZE,
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};


static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_smipool_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_smipool_pdata },
};


static struct platform_device android_pmem_kernel_ebi1_device = {
	.name = "android_pmem",
	.id = 3,
	.dev = { .platform_data = &android_pmem_kernel_ebi1_pdata },
};

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
static struct platform_device android_pmem_kernel_smi_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_kernel_smi_pdata },
};
#endif



/*
***************************************************************************************************
*    IDE
***************************************************************************************************
*/

/*
***************************************************************************************************
*    Frame Buffer
***************************************************************************************************
*/
static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

static int msm_fb_detect_panel(const char *name)
{
	int ret = -EPERM;

    if(!strcmp(name, "lcdc_sony_wvga"))
        ret = 0;

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = -1,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lcdc", 0);
}



/*
***************************************************************************************************
*    MSM Audio
***************************************************************************************************
*/
static struct resource msm_audio_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 68,
		.end    = 68,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 69,
		.end    = 69,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 70,
		.end    = 70,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 71,
		.end    = 71,
		.flags  = IORESOURCE_IO,
	},
	{
		.name	= "audio_base_addr",
		.start	= 0xa0700000,
		.end	= 0xa0700000 + 4,
		.flags	= IORESOURCE_MEM,
	},

};

// 20100524, moved to board-s1-rfkill.c
/*
static unsigned audio_gpio_on[] = {
	GPIO_CFG(S1_GPIO_AUX_PCM_DOUT, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_AUX_PCM_DIN, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_AUX_PCM_SYNC, 2, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
	GPIO_CFG(S1_GPIO_AUX_PCM_CLK, 2, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
};

static void __init audio_gpio_init(void)
{
	int pin, rc;

	for (pin = 0; pin < ARRAY_SIZE(audio_gpio_on); pin++) {
		rc = gpio_tlmm_config(audio_gpio_on[pin],
			GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR
				"%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, audio_gpio_on[pin], rc);
			return;
		}
	}
}
*/

static struct platform_device msm_audio_device = {
	.name   = "msm_audio",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_audio_resources),
	.resource       = msm_audio_resources,
};

#if 1
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= S1_GPIO_BT_HOST_WAKE,
		.end	= S1_GPIO_BT_HOST_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= S1_GPIO_BT_WAKE,	// ??
		.end	= S1_GPIO_BT_WAKE,	// ??
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(S1_GPIO_BT_HOST_WAKE),
		.end	= MSM_GPIO_TO_INT(S1_GPIO_BT_HOST_WAKE),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};
#endif


/*
***************************************************************************************************
*    KSGL
***************************************************************************************************
*/
static struct resource kgsl_resources[] = {
       {
		.name  = "kgsl_reg_memory",
		.start = 0xA0000000,
		.end = 0xA001ffff,
		.flags = IORESOURCE_MEM,
       },
       {
		.name   = "kgsl_phys_memory",
		.start = MSM_GPU_PHYS_BASE,
		.end = MSM_GPU_PHYS_BASE + MSM_GPU_PHYS_SIZE - 1,
		.flags = IORESOURCE_MEM,
       },
       {
		.name = "kgsl_yamato_irq",
		.start = INT_GRAPHICS,
		.end = INT_GRAPHICS,
		.flags = IORESOURCE_IRQ,
       },
};
static struct kgsl_platform_data kgsl_pdata = {
	.high_axi_3d = 128000, /* Max for 8K */
	.max_grp2d_freq = 0,
	.min_grp2d_freq = 0,
	.set_grp2d_async = NULL,
	.max_grp3d_freq = 0,
	.min_grp3d_freq = 0,
	.set_grp3d_async = NULL,
	.imem_clk_name = "imem_clk",
	.grp3d_clk_name = "grp_clk",
	.grp2d_clk_name = NULL,
};

static struct platform_device msm_device_kgsl = {
       .name = "kgsl",
       .id = -1,
       .num_resources = ARRAY_SIZE(kgsl_resources),
       .resource = kgsl_resources,
       .dev = {
		.platform_data = &kgsl_pdata,
	},
};

#if ((CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01) && defined(CONFIG_LEDS_MSM_PMIC))
static struct platform_device msm_device_pmic_leds = {
	.name	= "pmic-leds",
	.id	= -1,
};
#endif

/*
***************************************************************************************************
*    TSIF
***************************************************************************************************
*/
/* TSIF begin */
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)

#define TSIF_A_DATA	GPIO_CFG(S1_GPIO_TDMB_TSIF_DATA, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA)
#define TSIF_A_EN	GPIO_CFG(S1_GPIO_TDMB_TSIF_EN, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA)
#define TSIF_A_CLK	GPIO_CFG(S1_GPIO_TDMB_TSIF_CLK, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA)

static const struct msm_gpio tsif_gpios[] = {
	{ .gpio_cfg = TSIF_A_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_A_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_A_DATA, .label =  "tsif_data", },
#if 0
	{ .gpio_cfg = TSIF_A_SYNC, .label =  "tsif_sync", },
#endif
};

static struct msm_tsif_platform_data tsif_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif_gpios),
	.gpios = tsif_gpios,
	.tsif_clk = "tsif_clk",
	.tsif_ref_clk = "tsif_ref_clk",
};

#endif /* defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE) */
/* TSIF end   */



/*
***************************************************************************************************
*    Clock
***************************************************************************************************
*/
#ifdef CONFIG_QSD_SVS
#define TPS65023_MAX_DCDC1	1600
#else
#define TPS65023_MAX_DCDC1	CONFIG_QSD_PMIC_DEFAULT_DCDC1
#endif

static int s1_tps65023_set_dcdc1(int mVolts)
{
	int rc = 0;
#ifdef CONFIG_QSD_SVS
  #ifdef CONFIG_MACH_QSD8X50_S1
	int cnt;

	for (cnt = 5, rc = -1; cnt > 0 && rc; cnt--)
	{
		rc = tps65023_set_dcdc1_level(mVolts);
		/* By default the TPS65023 will be initialized to 1.225V.
		 * So we can safely switch to any frequency within this
		 * voltage even if the device is not probed/ready.
		 */
		if (rc == -ENODEV && mVolts <= CONFIG_QSD_PMIC_DEFAULT_DCDC1)
			rc = 0;

		if (rc)
		{
			printk(KERN_ERR "%s(%d) err %d !!!!! \n", __func__, mVolts, rc);
			msleep(100);
		}
	}
  #else
	rc = tps65023_set_dcdc1_level(mVolts);
	/* By default the TPS65023 will be initialized to 1.225V.
	 * So we can safely switch to any frequency within this
	 * voltage even if the device is not probed/ready.
	 */
	if (rc == -ENODEV && mVolts <= CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = 0;
  #endif
#else
	/* Disallow frequencies not supported in the default PMIC
	 * output voltage.
	 */
	if (mVolts > CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = -EFAULT;
#endif
	return rc;
}

static struct msm_acpu_clock_platform_data s1_clock_data = {
	.acpu_switch_time_us = 20,
	.max_speed_delta_khz = 256000,
#ifdef CONFIG_MACH_QSD8X50_S1
	.vdd_switch_time_us = 100,
#else
	.vdd_switch_time_us = 62,
#endif
	.max_vdd = TPS65023_MAX_DCDC1,
	.acpu_set_vdd = s1_tps65023_set_dcdc1,
};


/*
***************************************************************************************************
*    Touch PAD
***************************************************************************************************
*/
static void touchpad_gpio_release(void)
{
	gpio_free(S1_GPIO_TOUCHPAD_nINT);
	gpio_free(S1_GPIO_TOUCHPAD_nRESET);
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	gpio_free(S1_GPIO_5V_TOUCH_EN);
#endif
}

static int touchpad_gpio_setup(void)
{
	int rc;
	int reset_pin = S1_GPIO_TOUCHPAD_nRESET;
	int irq_pin = S1_GPIO_TOUCHPAD_nINT;
	unsigned reset_cfg =
		GPIO_CFG(reset_pin, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
	unsigned irq_cfg =
		GPIO_CFG(irq_pin, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	int pwr_en_pin = S1_GPIO_5V_TOUCH_EN;
	unsigned pwr_en_cfg =
		GPIO_CFG(pwr_en_pin, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
#endif

	rc = gpio_request(irq_pin, "cypress_touchpad_irq");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       irq_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(reset_pin, "cypress_touchpad_reset");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       reset_pin, rc);
		goto err_gpioconfig;
	}
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	rc = gpio_request(pwr_en_pin, "cypress_touchpad_pwr_en");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       pwr_en_pin, rc);
		goto err_gpioconfig;
	}
#endif
	rc = gpio_tlmm_config(reset_cfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
		       reset_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(irq_cfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
		       irq_pin, rc);
		goto err_gpioconfig;
	}
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	rc = gpio_tlmm_config(pwr_en_cfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
		       pwr_en_pin, rc);
		goto err_gpioconfig;
	}
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	gpio_set_value(reset_pin, 1);
#else
	gpio_set_value(reset_pin, 0);
#endif
	return rc;

err_gpioconfig:
	touchpad_gpio_release();
	return rc;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
static struct tma340_platform_data tma340_touchpad_data = {
	.gpioirq     = S1_GPIO_TOUCHPAD_nINT,
	.gpioreset = S1_GPIO_TOUCHPAD_nRESET,
	.init_platform_hw  = touchpad_gpio_setup,
	.exit_platform_hw = touchpad_gpio_release
};
#else
static struct tmg200_platform_data tmg200_touchpad_data = {
	.gpioirq     = S1_GPIO_TOUCHPAD_nINT,
	.gpioreset = S1_GPIO_TOUCHPAD_nRESET,
	.init_platform_hw  = touchpad_gpio_setup,
	.exit_platform_hw = touchpad_gpio_release
};
#endif

#ifdef CONFIG_BATTERY_BQ27505
/*
***************************************************************************************************
*    BQ27505 Fuel Gauge
***************************************************************************************************
*/
static unsigned gauge_cfg_data[] = {
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
	GPIO_CFG(S1_GPIO_BAT_nINT,      0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	GPIO_CFG(S1_GPIO_FUEL_GAUGE_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
#endif
};

static int bq27505_gpio_cfg(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(gauge_cfg_data); i++) {
		rc = gpio_tlmm_config(gauge_cfg_data[i], GPIO_ENABLE);
 		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, gauge_cfg_data[i], rc);
	}

	return rc;
}

static struct bq27505_platform_data bq27505_gauge_data = {
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	.pwr_pin       = S1_GPIO_FUEL_GAUGE_EN,
#else
	.pwr_pin       = -1,
#endif
	.init_gpio_cfg = bq27505_gpio_cfg,
};
#endif /* CONFIG_BATTERY_BQ27505 */


/*
***************************************************************************************************
*    Key PAD
***************************************************************************************************
*/


/*
***************************************************************************************************
*    PM, I2C, HW-I2c Only
***************************************************************************************************
*/
static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 8594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 4594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].residency = 23740,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].suspend_enabled
		= 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 443,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].residency = 1098,

	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].latency = 2,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].residency = 0,
};

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
static struct i2c_board_info msm_i2c_board_info[] __initdata = {
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02)
	{
		I2C_BOARD_INFO("tmg200", 0x20),
		.irq           =  MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT),
		.platform_data = &tmg200_touchpad_data
	},
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	{
		I2C_BOARD_INFO("tps65023", 0x48),
	},
#endif
};

static void
msm_i2c_gpio_config(int iface, int config_type)
{
	int gpio_scl;
	int gpio_sda;
	if (iface) {
//		gpio_scl = 60;
//		gpio_sda = 61;
	} else {
		gpio_scl = 95;
		gpio_sda = 96;
	}
	if (config_type) {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.rsl_id = SMEM_SPINLOCK_I2C,
	.pri_clk = 95,
	.pri_dat = 96,
//	.aux_clk = 60,
//	.aux_dat = 61,
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (gpio_request(95, "i2c_pri_clk"))
		pr_err("failed to request gpio i2c_pri_clk\n");
	if (gpio_request(96, "i2c_pri_dat"))
		pr_err("failed to request gpio i2c_pri_dat\n");
/*    
	if (gpio_request(60, "i2c_sec_clk"))
		pr_err("failed to request gpio i2c_sec_clk\n");
	if (gpio_request(61, "i2c_sec_dat"))
		pr_err("failed to request gpio i2c_sec_dat\n");
*/
	msm_i2c_pdata.rmutex = (uint32_t)smem_alloc(SMEM_I2C_MUTEX, 8);
	msm_i2c_pdata.pm_lat =
		msm_pm_data[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]
		.latency;
	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}
#endif

/*
***************************************************************************************************
*    GPIO-I2C
***************************************************************************************************
*/
const static uint i2c_gpios_cfg[] = 
{
	GPIO_CFG(S1_GPIO_AUDIO_SDA, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),     /* Audio AMP */
	GPIO_CFG(S1_GPIO_AUDIO_SCL, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_CAM_DMB_SDA, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),   /* CAM, DMB */
	GPIO_CFG(S1_GPIO_CAM_DMB_SCL, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SENSOR_SDA, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),    /* APDS, BMA Sensors */
	GPIO_CFG(S1_GPIO_SENSOR_SCL, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA),     /* TOUCH, TI PMIC */
	GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA),
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	GPIO_CFG(S1_GPIO_TPS_SCL, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),       /* TI PMIC */
	GPIO_CFG(S1_GPIO_TPS_SDA, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
#endif
};

static void i2c_gpios_init(void)
{
	int	i, rc;

	for (i = 0; i < ARRAY_SIZE(i2c_gpios_cfg); i++)
	{
		rc = gpio_tlmm_config(i2c_gpios_cfg[i], GPIO_ENABLE);
		if (rc) {
			pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
			       " failed: %d\n",
			       GPIO_PIN(i2c_gpios_cfg[i]), rc);
		}
	}
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
/*
===================================================================================================
=	Touch I2C, Ch1
===================================================================================================
*/
static struct i2c_gpio_platform_data i2c_gpio_touch_data = {
	.sda_pin	= S1_GPIO_TOUCH_SDA,
	.scl_pin	= S1_GPIO_TOUCH_SCL,
	.udelay		= 1,
};

static struct platform_device i2c_gpio_touch_device = {
	.name	= "i2c-gpio",
	.id	= 1,
	.dev	= {
		.platform_data	= &i2c_gpio_touch_data,
	},
};

static struct i2c_board_info i2c_touch_info[] __initdata = {
	{
		I2C_BOARD_INFO("tma340", 0x20),
		.irq           =  MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT),
		.platform_data = &tma340_touchpad_data
	},
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_ES00)
	{
		I2C_BOARD_INFO("tps65023", 0x48),
	},
#endif
};
#endif

/*
===================================================================================================
=	Audio I2C, Ch2
===================================================================================================
*/
static struct i2c_gpio_platform_data i2c_gpio_audio_data = {
	.sda_pin	= S1_GPIO_AUDIO_SDA,
	.scl_pin	= S1_GPIO_AUDIO_SCL,
	.udelay		= 5,    		/* for 100KHz */
};

static struct platform_device i2c_gpio_audio_device = {
	.name	= "i2c-gpio",
	.id		= 2,
	.dev	= {
		.platform_data	= &i2c_gpio_audio_data,
	},
};

static struct i2c_board_info i2c_audio_amp_info[] __initdata = {
	{
		I2C_BOARD_INFO("yda158", 0x6C),
	},
};

/*
===================================================================================================
=	TI_Power, Camera DMB I2C, Ch3
===================================================================================================
*/
static struct i2c_gpio_platform_data i2c_gpio_pcd_data = {
	.sda_pin	= S1_GPIO_CAM_DMB_SDA,
	.scl_pin	= S1_GPIO_CAM_DMB_SCL,
//	.udelay		= 5,    		/* for 100KHz */
	.udelay		= 2,    		/* for 400KHz */ // @cam : for 5m camera
};

static struct platform_device i2c_gpio_pcd_device = {
	.name	= "i2c-gpio",
	.id	= 3,
	.dev	= {
		.platform_data	= &i2c_gpio_pcd_data,
	},
};

static struct i2c_board_info i2c_pcd_info[] __initdata = {
#if (CONFIG_S1_BOARD_VER < S1_BOARD_VER_ES00)
	{
		I2C_BOARD_INFO("tps65023", 0x48),	// TI Power
	},
#endif
	{
		I2C_BOARD_INFO("isx006", 0x1A),		// @cam : Sony 5M sensor
	},
	{
		I2C_BOARD_INFO("mt9v113", 0x3C),	// @cam : Micron VGA sensor
	},
	{
		I2C_BOARD_INFO("tdmb", 0x40),
	},
#ifdef CONFIG_BATTERY_BQ27505
	{
		I2C_BOARD_INFO("bq27505", 0x55),
		.irq           = MSM_GPIO_TO_INT(S1_GPIO_BAT_nINT),
		.platform_data = &bq27505_gauge_data,
	},
#endif /* CONFIG_BATTERY_BQ27505 */
};


/*
===================================================================================================
=	Sensor I2C, Ch4
===================================================================================================
*/
#define ACCELERATION_INTR    42
static struct i2c_gpio_platform_data i2c_gpio_sensor_data = {
	.sda_pin	= S1_GPIO_SENSOR_SDA,
	.scl_pin	= S1_GPIO_SENSOR_SCL,
	.udelay		= 5,    		/* for 100KHz */
};

static struct platform_device i2c_gpio_sensor_device = {
	.name	= "i2c-gpio",
	.id	= 4,
	.dev	= {
		.platform_data	= &i2c_gpio_sensor_data,
	},
};

static struct i2c_board_info i2c_sensor_info[] __initdata = {
	{
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02)
		I2C_BOARD_INFO("apds", 0x56),	/* APDS9180, Proximity sensor */
#else
		I2C_BOARD_INFO("ltr-505", 0x1c),	/* Light-On, Proximity sensor */
#endif
	},
	{
		I2C_BOARD_INFO("bma", 0x38),	/* BMA150                     */
		.irq =  MSM_GPIO_TO_INT(ACCELERATION_INTR),
	},
	{
		I2C_BOARD_INFO("ams0303", 0x28),	/* amotek Magnetic Field  Sensor */
	}
};


#if 0// (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
/*
===================================================================================================
=	Tps(Sub PM) I2C, Ch4
===================================================================================================
*/
static struct i2c_gpio_platform_data i2c_gpio_tps_data = {
	.sda_pin	= S1_GPIO_TPS_SDA,
	.scl_pin	= S1_GPIO_TPS_SCL,
	.udelay		= 5,    		/* for 100KHz */
};

static struct platform_device i2c_gpio_tps_device = {
	.name	= "i2c-gpio",
	.id	= 4,
	.dev	= {
		.platform_data	= &i2c_gpio_tps_data,
	},
};

static struct i2c_board_info i2c_tps_info[] __initdata = {
	{
		I2C_BOARD_INFO("tps65023", 0x48),
	},
};
#endif


/*
***************************************************************************************************
*    Camera
***************************************************************************************************
*/

#ifdef CONFIG_MSM_CAMERA
static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
#if 0
	GPIO_CFG(0,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(3,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
#endif
	GPIO_CFG(4,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_WS01)
	GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* MCLK */
#endif
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
#if 0
	GPIO_CFG(0,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(3,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
#endif
	GPIO_CFG(4,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_WS01)
	GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA), /* MCLK */
#endif
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

static struct resource msm_camera_resources[] = {
	{
		.start	= 0xA0F00000,
		.end	= 0xA0F00000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PMIC,
	._fsrc.pmic_src.low_current  = 30,
	._fsrc.pmic_src.high_current = 100,
};

#ifdef CONFIG_ISX006
static struct msm_camera_sensor_flash_data flash_isx006 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_isx006_data = {
	.sensor_name    = "isx006",
	.sensor_reset   = S1_GPIO_MAIN_CAM_nRST,
	.sensor_pwd     = 0,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_isx006
};

static struct platform_device msm_camera_sensor_isx006 = {
	.name      = "msm_camera_isx006",
	.dev       = {
		.platform_data = &msm_camera_sensor_isx006_data,
	},
};
#endif

#ifdef CONFIG_MT9V113
static struct msm_camera_sensor_flash_data flash_mt9v113 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9v113_data = {
	.sensor_name    = "mt9v113",
	.sensor_reset   = S1_GPIO_SUB_CAM_nRST,
	.sensor_pwd     = 0,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_mt9v113
};

static struct platform_device msm_camera_sensor_mt9v113 = {
	.name      = "msm_camera_mt9v113",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9v113_data,
	},
};
#endif
#endif /*CONFIG_MSM_CAMERA*/


/*
***************************************************************************************************
*    Battery
***************************************************************************************************
*/
static vbatt_capacity_table_type vbatt_capacity_lookup [VBATT_TABLE_SIZE] = 
{
	/* CAPACITY       VOLTAGE   */
	{ BATT_00_PERCENT ,  3300 },
	{ BATT_05_PERCENT ,  3359 },
	{ BATT_10_PERCENT ,  3557 },
	{ BATT_15_PERCENT ,  3607 },
	{ BATT_20_PERCENT ,  3656 },
	{ BATT_25_PERCENT ,  3672 },
	{ BATT_30_PERCENT ,  3689 },
	{ BATT_35_PERCENT ,  3705 },
	{ BATT_40_PERCENT ,  3722 },
	{ BATT_45_PERCENT ,  3738 },
	{ BATT_50_PERCENT ,  3755 },
	{ BATT_55_PERCENT ,  3788 },
	{ BATT_60_PERCENT ,  3804 },
	{ BATT_65_PERCENT ,  3837 },
	{ BATT_70_PERCENT ,  3870 },
	{ BATT_75_PERCENT ,  3903 },
	{ BATT_80_PERCENT ,  3936 },
	{ BATT_85_PERCENT ,  3985 },
	{ BATT_90_PERCENT ,  4035 },
	{ BATT_95_PERCENT ,  4084 },
	{ BATT_100_PERCENT,  4101 }
};

static u32 msm_calculate_batt_capacity(u32 current_voltage);
static void battery_config_gpio(void);

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design = 3300,
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	.voltage_max_design	= 4150,
#else
	.voltage_max_design	= 4101,
#endif
	.avail_chg_sources  = AC_CHG | USB_CHG ,
	.batt_technology    = POWER_SUPPLY_TECHNOLOGY_LION,
	.calculate_capacity	= &msm_calculate_batt_capacity,
#ifdef CONFIG_MACH_QSD8X50_S1
	.config_gpio = &battery_config_gpio
#endif
};

static u32 msm_calculate_batt_capacity(u32 current_voltage)
{
	int iterator;
	u32 low_voltage, high_voltage;

	/* Set it to 0 */
	vbatt_capacity_type vbatt_cap_from_tbl = BATT_00_PERCENT;

	/* Traverse from the end to the begining */
	iterator = ( GET_VBATT_TABLE_SIZE() - 1 ) ; /* Last index */

	do
	{   /* If a larger entry  is found, job done. */
		if ( current_voltage >=
			vbatt_capacity_lookup[iterator].vbatt_translation_voltage )
		{
			/* Get the corresponding percent and quit */
			vbatt_cap_from_tbl =
				vbatt_capacity_lookup[iterator].vbatt_capacity_percentage;
			break;
		} 
	} while ( iterator-- > 0 );

	if (vbatt_cap_from_tbl == BATT_00_PERCENT && iterator == 0)
	{
		low_voltage   = vbatt_capacity_lookup[0].vbatt_translation_voltage;
		high_voltage  = vbatt_capacity_lookup[1].vbatt_translation_voltage;
		vbatt_cap_from_tbl += 
			(current_voltage - low_voltage) * 5 / (high_voltage - low_voltage);
	}

	return vbatt_cap_from_tbl;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
static struct msm_gpio battery_gpio_config_data[] = {
	{ GPIO_CFG(S1_GPIO_CHARGER_nDET, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), "charger_det" },
	{ GPIO_CFG(S1_GPIO_TTA_nDET, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), "tta_det" },
	{ GPIO_CFG(S1_GPIO_CHARGER_STATUS, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), "charger_status" },
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	{ GPIO_CFG(S1_GPIO_CHARGER_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), "charger_en" },
#endif
};
#endif

static void battery_config_gpio(void)
{
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
	msm_gpios_request_enable(battery_gpio_config_data,
				ARRAY_SIZE(battery_gpio_config_data));
#endif
}

static struct platform_device msm_batt_device = {
	.name 		    = "msm-battery",
	.id		    = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

/*
***************************************************************************************************
*    WiFi
***************************************************************************************************
*/
static struct platform_device s1_rfkill = {
	.name = "s1_rfkill",
	.id = -1,
};



/*
***************************************************************************************************
*    SDCC
***************************************************************************************************
*/
static void sdcc_gpio_init(void)
{
	/* SDC1 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	if (gpio_request(S1_GPIO_SD_nDET, "sdc1_cd_detect"))
		pr_err("failed to request gpio sdc1_cd_detect\n");
	if (gpio_request(S1_GPIO_SD_D3, "sdc1_data_3"))
		pr_err("failed to request gpio sdc1_data_3\n");
	if (gpio_request(S1_GPIO_SD_D2, "sdc1_data_2"))
		pr_err("failed to request gpio sdc1_data_2\n");
	if (gpio_request(S1_GPIO_SD_D1, "sdc1_data_1"))
		pr_err("failed to request gpio sdc1_data_1\n");
	if (gpio_request(S1_GPIO_SD_D0, "sdc1_data_0"))
		pr_err("failed to request gpio sdc1_data_0\n");
	if (gpio_request(S1_GPIO_SD_CMD, "sdc1_cmd"))
		pr_err("failed to request gpio sdc1_cmd\n");
	if (gpio_request(S1_GPIO_SD_CLK, "sdc1_clk"))
		pr_err("failed to request gpio sdc1_clk\n");
#endif

	/* SDC2 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	if (gpio_request(S1_GPIO_WIFI_CLK, "sdc2_clk"))
		pr_err("failed to request gpio sdc2_clk\n");
	if (gpio_request(S1_GPIO_WIFI_CMD, "sdc2_cmd"))
		pr_err("failed to request gpio sdc2_cmd\n");
	if (gpio_request(S1_GPIO_WIFI_D3, "sdc2_data_3"))
		pr_err("failed to request gpio sdc2_data_3\n");
	if (gpio_request(S1_GPIO_WIFI_D2, "sdc2_data_2"))
		pr_err("failed to request gpio sdc2_data_2\n");
	if (gpio_request(S1_GPIO_WIFI_D1, "sdc2_data_1"))
		pr_err("failed to request gpio sdc2_data_1\n");
	if (gpio_request(S1_GPIO_WIFI_D0, "sdc2_data_0"))
		pr_err("failed to request gpio sdc2_data_0\n");
#endif

	/* SDC3 GPIOs */
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	if (gpio_request(S1_GPIO_TRJ_CLK, "sdc3_clk"))
		pr_err("failed to request gpio sdc3_clk\n");
	if (gpio_request(S1_GPIO_TRJ_CMD, "sdc3_cmd"))
		pr_err("failed to request gpio sdc3_cmd\n");
	if (gpio_request(S1_GPIO_TRJ_D3, "sdc3_data_3"))
		pr_err("failed to request gpio sdc3_data_3\n");
	if (gpio_request(S1_GPIO_TRJ_D2, "sdc3_data_2"))
		pr_err("failed to request gpio sdc3_data_2\n");
	if (gpio_request(S1_GPIO_TRJ_D1, "sdc3_data_1"))
		pr_err("failed to request gpio sdc3_data_1\n");
	if (gpio_request(S1_GPIO_TRJ_D0, "sdc3_data_0"))
		pr_err("failed to request gpio sdc3_data_0\n");
#endif
}

static unsigned sdcc_cfg_data[][6] = {
	/* SDC1 configs */
	{
		GPIO_CFG(S1_GPIO_SD_D3,    1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_SD_D2,    1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_SD_D1,    1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_SD_D0,    1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_SD_CMD,   1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_SD_CLK,   1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	},
	/* SDC2 configs, this configuration is not used */
	{
		GPIO_CFG(S1_GPIO_WIFI_CLK, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_WIFI_CMD, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_WIFI_D3,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_WIFI_D2,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_WIFI_D1,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_WIFI_D0,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
#ifdef CONFIG_S1_TRANSFERJET
	/* SDC3 configs */
	{
		GPIO_CFG(S1_GPIO_TRJ_CLK,  1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
		GPIO_CFG(S1_GPIO_TRJ_CMD,  1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_TRJ_D3,   1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_TRJ_D2,   1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_TRJ_D1,   1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
		GPIO_CFG(S1_GPIO_TRJ_D0,   1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
#endif
};

static unsigned sdcc_decfg_data[][6] = {
	/* SDC1 configs */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	{
		GPIO_CFG(S1_GPIO_SD_D3,    0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D2,    0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D1,    0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D0,    0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_CMD,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_CLK,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
	},
#else
	{
		GPIO_CFG(S1_GPIO_SD_D3,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D2,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D1,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_D0,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_CMD,   0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
		GPIO_CFG(S1_GPIO_SD_CLK,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
	},
#endif
	/* SDC2 configs */
	{
		GPIO_CFG(S1_GPIO_WIFI_CLK, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_WIFI_CMD, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_WIFI_D3,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_WIFI_D2,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_WIFI_D1,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_WIFI_D0,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
	},
#ifdef CONFIG_S1_TRANSFERJET
	/* SDC3 configs */
	{
		GPIO_CFG(S1_GPIO_TRJ_CLK,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_TRJ_CMD,  0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_TRJ_D3,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_TRJ_D2,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_TRJ_D1,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
		GPIO_CFG(S1_GPIO_TRJ_D0,   0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
	},
#endif
};

static unsigned long gpio_sts;

static void msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int i, rc;

	if (!(test_bit(dev_id, &gpio_sts)^(enable ? 1 : 0)))
		return;

	if (enable)
		set_bit(dev_id, &gpio_sts);
	else
		clear_bit(dev_id, &gpio_sts);

	for (i = 0; i < ARRAY_SIZE(sdcc_cfg_data[dev_id - 1]); i++) {
		if (enable)
			rc = gpio_tlmm_config(sdcc_cfg_data[dev_id - 1][i], GPIO_ENABLE);
		else
			rc = gpio_tlmm_config(sdcc_decfg_data[dev_id - 1][i], GPIO_DISABLE);
 		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, sdcc_cfg_data[dev_id - 1][i], rc);
	}
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, vdd);

	if (pdev->id == 1)	/* SD-Card */
	{
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
		gpio_set_value(S1_GPIO_TFLASH_EN, vdd ? 1 : 0);
#else
		// do not off sd_pwr because card detect signal is pulled-up to this power
		gpio_set_value(S1_GPIO_EN_SD_PWR, 1);
#endif
	}
#ifdef CONFIG_S1_TRANSFERJET
	else if (pdev->id == 3)
	{
		gpio_set_value(S1_GPIO_TRJ_EN, 1);
	}
#endif

	return 0;
}

#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
static unsigned int s1_sdslot_status(struct device *dev)
{
	unsigned int status;

	status = (unsigned int) gpio_get_value(S1_GPIO_SD_nDET);
	return !status;
}
#endif

static struct mmc_platform_data s1_sdcc_sd = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	.status         = s1_sdslot_status,
	.status_irq     = MSM_GPIO_TO_INT(S1_GPIO_SD_nDET),
	.irq_flags      = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#endif
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#ifdef CONFIG_MMC_MSM_SDC1_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};

#ifdef CONFIG_S1_TRANSFERJET
extern unsigned int s1_trj_status(struct device *dev);
extern int s1_trj_status_register(void (*callback)(int card_present, void *dev_id), void *dev_id);

static struct mmc_platform_data s1_trj_data = {
	.ocr_mask		= MMC_VDD_28_29,
	.translate_vdd	= msm_sdcc_setup_power,
	.status			= s1_trj_status,
	.register_status_notify	= s1_trj_status_register,
	.mmc_bus_width	= MMC_CAP_4_BIT_DATA,
	.dummy52_required = 1,
};

#endif

extern struct mmc_platform_data s1_wifi_data;

static void __init s1_init_mmc(void)
{
	sdcc_gpio_init();
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	set_irq_wake(MSM_GPIO_TO_INT(S1_GPIO_SD_nDET), 1);
#endif
	
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	msm_add_sdcc(1, &s1_sdcc_sd);
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	msm_add_sdcc(2, &s1_wifi_data);
#endif

#ifdef CONFIG_S1_TRANSFERJET
	msm_add_sdcc(3, &s1_trj_data);
#endif
}


static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}

static struct msm_otg_platform_data msm_otg_pdata = {
	.rpc_connect	= hsusb_rpc_connect,
	.pmic_notif_init         = msm_pm_app_rpc_init,
	.pmic_notif_deinit       = msm_pm_app_rpc_deinit,
	.pmic_register_vbus_sn   = msm_pm_app_register_vbus_sn,
	.pmic_unregister_vbus_sn = msm_pm_app_unregister_vbus_sn,
	.pmic_enable_ldo         = msm_pm_app_enable_usb_ldo,
	.pemp_level              = PRE_EMPHASIS_WITH_10_PERCENT,
	.cdr_autoreset           = CDR_AUTO_RESET_DEFAULT,
	.drv_ampl                = HS_DRV_AMPLITUDE_5_PERCENT,
};

static void usb_mpp_init(void)
{
	unsigned rc;
#ifdef CONFIG_MACH_QSD8X50_S1
	unsigned mpp_usb = 6; /* PM_MPP_7 */
#else
	unsigned mpp_usb = 20;
#endif

		rc = mpp_config_digital_out(mpp_usb,
			MPP_CFG(MPP_DLOGIC_LVL_VDD,
				MPP_DLOGIC_OUT_CTRL_HIGH));
		if (rc)
			pr_err("%s: configuring mpp pin"
				"to enable 3.3V LDO failed\n", __func__);
}
static void __init s1_init_usb(void)
{
	usb_mpp_init();

#ifdef CONFIG_USB_MSM_OTG_72K
	platform_device_register(&msm_device_otg);
#endif

#ifdef CONFIG_USB_FUNCTION_MSM_HSUSB
	platform_device_register(&msm_device_hsusb_peripheral);
#endif

#ifdef CONFIG_USB_MSM_72K
	platform_device_register(&msm_device_gadget_peripheral);
#endif

	vreg_usb = vreg_get(NULL, "boost");

	if (IS_ERR(vreg_usb)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_usb));
		return;
	}

	platform_device_register(&msm_device_hsusb_otg);
	msm_add_host(0, &msm_usb_host_pdata);
#ifdef CONFIG_USB_FS_HOST
	if (fsusb_gpio_init())
		return;
	msm_add_host(1, &msm_usb_host2_pdata);
#endif
}

static void __init s1_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

static void kgsl_phys_memory_init(void)
{
	request_mem_region(kgsl_resources[1].start,
		resource_size(&kgsl_resources[1]), "kgsl");
}
/*
***************************************************************************************************
*    Early Params
***************************************************************************************************
*/
static unsigned pmem_kernel_ebi1_size = PMEM_KERNEL_EBI1_SIZE;
static void __init pmem_kernel_ebi1_size_setup(char **p)
{
	pmem_kernel_ebi1_size = memparse(*p, p);
}
__early_param("pmem_kernel_ebi1_size=", pmem_kernel_ebi1_size_setup);


static unsigned pmem_sf_size = MSM_PMEM_SF_SIZE;
static void __init pmem_mdp_size_setup(char **p)
{
	pmem_sf_size = memparse(*p, p);
}
__early_param("pmem_mdp_size=", pmem_mdp_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
static void __init pmem_adsp_size_setup(char **p)
{
	pmem_adsp_size = memparse(*p, p);
}
__early_param("pmem_adsp_size=", pmem_adsp_size_setup);


static unsigned audio_size = MSM_AUDIO_SIZE;
static void __init audio_size_setup(char **p)
{
	audio_size = memparse(*p, p);
}
__early_param("audio_size=", audio_size_setup);

static struct platform_device *early_devices[] __initdata = {
#ifdef CONFIG_GPIOLIB
	&msm_gpio_devices[0],
	&msm_gpio_devices[1],
	&msm_gpio_devices[2],
	&msm_gpio_devices[3],
	&msm_gpio_devices[4],
	&msm_gpio_devices[5],
	&msm_gpio_devices[6],
	&msm_gpio_devices[7],
#endif
};

/*
***************************************************************************************************
*    platform_device
***************************************************************************************************
*/
static struct platform_device *devices[] __initdata = {
	&msm_fb_device,
	&msm_device_smd,
	&msm_device_dmov,
	&android_pmem_kernel_ebi1_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_smipool_device,
	&msm_device_nand,
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	&msm_device_i2c,
#endif
#ifdef CONFIG_USB_FUNCTION
	&mass_storage_device,
#endif
#ifdef CONFIG_USB_ANDROID
	&mass_storage_device,
	&android_usb_device,
#endif
//	&msm_device_tssc, // 20100808, not used in S1
	&msm_audio_device,
	&msm_device_uart_dm1,
	&msm_device_uart_dm2,
	&msm_bluesleep_device,

#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart3,
#endif
#if ((CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01) && defined(CONFIG_LEDS_MSM_PMIC))
	&msm_device_pmic_leds,
#endif	
	&msm_device_kgsl,
	&hs_device,
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
	&msm_device_tsif,
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	&i2c_gpio_touch_device,
#endif
	&i2c_gpio_audio_device,
	&i2c_gpio_pcd_device,
	&i2c_gpio_sensor_device,
#if 0// (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	&i2c_gpio_tps_device,
#endif

#ifdef CONFIG_ISX006
	&msm_camera_sensor_isx006,
#endif
#ifdef CONFIG_MT9V113
	&msm_camera_sensor_mt9v113,
#endif

	&msm_batt_device,
	&s1_rfkill,
};

extern int s1_init_wifi_mem (void);
static struct msm_hsusb_gadget_platform_data msm_gadget_pdata;

static void __init s1_init(void)
{
	if (socinfo_init() < 0)
		printk(KERN_ERR "%s: socinfo_init() failed!\n",
		       __func__);
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);
	platform_add_devices(early_devices, ARRAY_SIZE(early_devices));
	msm_acpu_clock_init(&s1_clock_data);

	msm_hsusb_pdata.swfi_latency =
		msm_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;

	msm_gadget_pdata.swfi_latency =
		msm_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_otg.dev.platform_data = &msm_otg_pdata;
	msm_device_gadget_peripheral.dev.platform_data = &msm_gadget_pdata;

#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
	msm_device_tsif.dev.platform_data = &tsif_platform_data;
#endif

	platform_add_devices(devices, ARRAY_SIZE(devices));
	msm_fb_add_devices();
    
#ifdef CONFIG_MSM_CAMERA
	config_camera_off_gpios(); /* might not be necessary */
#endif

	s1_init_usb();
	s1_init_mmc();
//	audio_gpio_init();  20100524, moved to board-s1-rfkill.c
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	msm_device_i2c_init();
#endif
	i2c_gpios_init();

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	i2c_register_board_info(0, msm_i2c_board_info,
				ARRAY_SIZE(msm_i2c_board_info));
#endif

	i2c_register_board_info(1, i2c_touch_info,
				ARRAY_SIZE(i2c_touch_info));

	i2c_register_board_info(2, i2c_audio_amp_info,
				ARRAY_SIZE(i2c_audio_amp_info));

	i2c_register_board_info(3, i2c_pcd_info,
				ARRAY_SIZE(i2c_pcd_info));

	i2c_register_board_info(4, i2c_sensor_info,
				ARRAY_SIZE(i2c_sensor_info));

#if 0// (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	i2c_register_board_info(4, i2c_tps_info,
				ARRAY_SIZE(i2c_tps_info));
#endif

	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	kgsl_phys_memory_init();

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
	msm_init_gpio_vibrator();
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
	msm_init_pmic_vibrator();
#endif
}

static void __init s1_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	size = pmem_kernel_ebi1_size;
	if (size) {
		addr = alloc_bootmem_aligned(size, 0x100000);
		android_pmem_kernel_ebi1_pdata.start = __pa(addr);
		android_pmem_kernel_ebi1_pdata.size = size;
		pr_err("allocating %lu bytes at %p (%lx physical) for kernel"
			" ebi1 pmem arena\n", size, addr, __pa(addr));
	}

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
	size = pmem_kernel_smi_size;
	if (size > MSM_PMEM_SMIPOOL_SIZE) {
		printk(KERN_ERR "pmem kernel smi arena size %lu is too big\n",
			size);

		size = MSM_PMEM_SMIPOOL_SIZE;
	}

	android_pmem_kernel_smi_pdata.start = MSM_PMEM_SMIPOOL_BASE;
	android_pmem_kernel_smi_pdata.size = size;

	pr_info("allocating %lu bytes at %lx (%lx physical)"
		"for pmem kernel smi arena\n", size,
		(long unsigned int) MSM_PMEM_SMIPOOL_BASE,
		__pa(MSM_PMEM_SMIPOOL_BASE));
#endif

	size = pmem_sf_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_pdata.start = __pa(addr);
		android_pmem_pdata.size = size;
		pr_err("allocating %lu bytes at %p (%lx physical) for sf "
			"pmem arena\n", size, addr, __pa(addr));
	}

	size = pmem_adsp_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_adsp_pdata.start = __pa(addr);
		android_pmem_adsp_pdata.size = size;
		pr_err("allocating %lu bytes at %p (%lx physical) for adsp "
			"pmem arena\n", size, addr, __pa(addr));
	}


	size = MSM_FB_SIZE;
	addr = (void *)MSM_FB_BASE;
	msm_fb_resources[0].start = (unsigned long)addr;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_err("using %lu bytes of SMI at %lx physical for fb\n",
	       size, (unsigned long)addr);

	size = audio_size ? : MSM_AUDIO_SIZE;
	addr = alloc_bootmem(size);
	msm_audio_resources[0].start = __pa(addr);
	msm_audio_resources[0].end = msm_audio_resources[0].start + size - 1;
	pr_err("allocating %lu bytes at %p (%lx physical) for audio\n",
		size, addr, __pa(addr));
}

static void __init s1_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_qsd8x50_io();
	s1_allocate_memory_regions();
}

//MACHINE_START(QSD8X50_SURF, "QCT QSD8X50 SURF")
MACHINE_START(QSD8X50_S1, "QCT QSD8X50 S1")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = s1_map_io,
	.init_irq = s1_init_irq,
	.init_machine = s1_init,
	.timer = &msm_timer,
MACHINE_END

