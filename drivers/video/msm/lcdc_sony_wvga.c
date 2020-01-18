/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include <mach/pmic.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>
#include "msm_fb.h"


struct sony_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};


static struct sony_state_type sony_state = { TRUE, TRUE, TRUE };



#if 0
static uint32_t lcdc_gpio_table[] = {
	GPIO_CFG(S1_GPIO_SPI_CLK,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SPI_nCS,    0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SPI_MOSI,   0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SPI_MISO,   0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_LCD_BL,     0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(S1_GPIO_LCD_nRESET, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n, rc;
	int pin;

	for (n = 0; n < len; n++) 
	{
		pin = GPIO_PIN(table[n]);
		if (enable)
		{
			rc = gpio_request(pin, "lcd_toshiba");
			if (rc) 
			{
				pr_err("gpio_request failed on pin %d (rc=%d)\n", pin, rc);
				break;
			}
		}

		rc = gpio_tlmm_config(table[n], enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc) 
		{
			pr_err("%s: gpio_tlmm_config(%d)=%d\n", __func__, pin, rc);
			break;
		}

		if (!enable)
		{
			gpio_free(pin);
		}
	}
}
#endif


static void sony_spi_write_byte(char dc, uint8 data)
{
	uint32 bit;
	int bnum;

	gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
	/* dc: 0 for command, 1 for parameter */
	gpio_set_value(S1_GPIO_SPI_MOSI, dc);
	udelay(1);	/* at least 30 ns */
	gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
	udelay(1);	/* at least 30 ns */
    
	bnum = 8;	/* 8 data bits */
	bit = 0x80;
	while (bnum) {
		gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
		if (data & bit)
			gpio_set_value(S1_GPIO_SPI_MOSI, 1);
		else
			gpio_set_value(S1_GPIO_SPI_MOSI, 0);
		udelay(1);
		gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
		udelay(1);
		bit >>= 1;
		bnum--;
	}
}


static void sony_spi_write(char cmd, uint32 data, int num)
{
	char *bp;

	gpio_set_value(S1_GPIO_SPI_nCS, 0);	/* cs low */

	/* command byte first */
	sony_spi_write_byte(0, cmd);

	/* followed by parameter bytes */
	if (num) {
		bp = (char *)&data;;
		bp += (num - 1);
		while (num) {
			sony_spi_write_byte(1, *bp);
			num--;
			bp--;
		}
	}

	gpio_set_value(S1_GPIO_SPI_nCS, 1);	/* cs high */
	udelay(1);
}

#if 0
void sony_spi_read_long(char cmd, uint32 *data, int num)
{
	uint32 dbit, bits;
	int bnum;

	gpio_set_value(S1_GPIO_SPI_nCS, 0);	/* cs low */

	/* command byte first */
	sony_spi_write_byte(0, cmd);

	if (num > 1) {
		/* extra dc bit */
		gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
		udelay(1);
		dbit = gpio_get_value(S1_GPIO_SPI_MISO);/* dc bit */
		udelay(1);
		gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
	}

	/* followed by data bytes */
	bnum = num * 8;	/* number of bits */
	bits = 0;
	while (bnum) {
		bits <<= 1;
		gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
		udelay(1);
		dbit = gpio_get_value(S1_GPIO_SPI_MISO);
		udelay(1);
		gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
		bits |= dbit;
		bnum--;
	}

	*data = bits;

	udelay(1);
	gpio_set_value(S1_GPIO_SPI_nCS, 1);	/* cs high */
	udelay(1);
}

void sony_spi_read_bytes(char cmd, uint8 *data, int num)
{
	uint32 dbit, bits;
	int bnum, i;

	gpio_set_value(S1_GPIO_SPI_nCS, 0);	/* cs low */

	/* command byte first */
	sony_spi_write_byte(0, cmd);

	if (num > 1) {
		/* extra dc bit */
		gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
		udelay(1);
		dbit = gpio_get_value(S1_GPIO_SPI_MISO);/* dc bit */
		udelay(1);
		gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
	}

	for (i = 0; i < num; i++)
	{
		/* followed by data bytes */
		bnum = 8;	/* number of bits */
		bits = 0;
		while (bnum) 
		{
			bits <<= 1;
			gpio_set_value(S1_GPIO_SPI_CLK, 0); /* clk low */
			udelay(1);
			dbit = gpio_get_value(S1_GPIO_SPI_MISO);
			udelay(1);
			gpio_set_value(S1_GPIO_SPI_CLK, 1); /* clk high */
			bits |= dbit;
			bnum--;
		}
		*data++ = bits;
	}

	udelay(1);
	gpio_set_value(S1_GPIO_SPI_nCS, 1);	/* cs high */
	udelay(1);
}
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
static int disp_power(int enable)
{
	struct vreg 	*vreg_lcd;
	int		rc = 0;

	gpio_set_value(S1_GPIO_LCD_nRESET, 0);
	vreg_lcd = vreg_get(NULL, "rfrx2");
	if (IS_ERR(vreg_lcd)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_lcd));
		goto exit;
	}

	if (enable)
	{
		/* units of mV, steps of 50 mV */
		rc = vreg_set_level(vreg_lcd, S1_LCD_VREG_LEVEL);
		if (rc) {
			printk(KERN_ERR "%s: vreg lcd set level failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		rc = vreg_enable(vreg_lcd);
		if (rc) {
			printk(KERN_ERR "%s: vreg lcd enable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}

		mdelay(5);
		gpio_set_value(S1_GPIO_LCD_nRESET, 1);
		mdelay(5);
	}
	else
	{
		rc = vreg_set_level(vreg_lcd, 0);
		if (rc) {
			printk(KERN_ERR "%s: vreg lcd set level failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		rc = vreg_disable(vreg_lcd);
		if (rc) {
			printk(KERN_ERR "%s: vreg lcd disable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
	}
	printk(KERN_DEBUG "lcd power switch: %s\n", enable ? "on" : "off");

exit:
	return (rc);
}
#endif


static void sony_disp_powerup(void)
{
	if (!sony_state.disp_powered_up && !sony_state.display_on) 
	{
		/* Reset the hardware first */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
		disp_power(1);
#endif		
		gpio_set_value(S1_GPIO_SPI_nCS, 1);
		gpio_set_value(S1_GPIO_SPI_CLK, 1);
		gpio_set_value(S1_GPIO_SPI_MOSI, 0);
		gpio_set_value(S1_GPIO_SPI_MISO, 0);
		//mdelay(200);  
		mdelay(20);
		/* Include DAC power up implementation here */
		sony_state.disp_powered_up = TRUE;
	}
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
static void sony_disp_powerdown(void)
{
	if (sony_state.disp_powered_up || sony_state.display_on) 
	{
		gpio_set_value(S1_GPIO_LCD_nRESET, 0);
		gpio_set_value(S1_GPIO_SPI_nCS, 0);
		gpio_set_value(S1_GPIO_SPI_CLK, 0);
		gpio_set_value(S1_GPIO_SPI_MOSI, 0);
		gpio_set_value(S1_GPIO_SPI_MISO, 0);

		disp_power(0);
		sony_state.disp_powered_up = FALSE;
	}
}
#endif

static void sony_disp_on(void)
{
	if (sony_state.disp_powered_up && !sony_state.display_on) 
	{
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
		sony_spi_write(0x3A, 0x60, 1);  /* LCD format : RGB666 18bit */
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_PT02)
		//sony_spi_write(0x3A, 0x70, 1);	/* LCD format : RGB888 24bit */
		/* LCD refresh Bottom to Top  = 0x10 */
		/* LCD refresh Right  to Left = 0x04 */

		/* Color Buffer Order : BGR   = 0x08 */
		/* Color Buffer Order : RGB   = 0x00 for FROYO*/
		sony_spi_write(0x36, 0x00, 1);
#endif
		gpio_set_value(S1_GPIO_SPI_nCS, 0);
		udelay(1);
		sony_spi_write_byte(0, 0x11);
		gpio_set_value(S1_GPIO_SPI_nCS, 1);
		mdelay(10);
        
		gpio_set_value(S1_GPIO_SPI_nCS, 0);
		udelay(1);
		sony_spi_write_byte(0, 0x29);
		gpio_set_value(S1_GPIO_SPI_nCS, 1);
		udelay(1);
        
		sony_state.display_on = TRUE;
	}
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)  
static void sony_disp_off(void)
{
	if (sony_state.disp_powered_up && sony_state.display_on) 
	{
		gpio_set_value(S1_GPIO_SPI_nCS, 0);
		udelay(1);
		sony_spi_write_byte(0, 0x28);
		sony_spi_write_byte(0, 0x10);
		gpio_set_value(S1_GPIO_SPI_nCS, 1);
		udelay(1);
        
		sony_state.display_on = FALSE;
	}
}
#endif


static int lcdc_sony_panel_on(struct platform_device *pdev)
{
	if (!sony_state.disp_initialized) 
	{
		printk(KERN_WARNING "%s\n", __func__);
		sony_disp_powerup();
		sony_disp_on();
		sony_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_sony_panel_off(struct platform_device *pdev)
{
	if (sony_state.disp_powered_up && sony_state.display_on) 
	{
		/* Main panel power off (Deep standby in) */
		printk(KERN_WARNING "%s\n", __func__);
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
		sony_disp_off();
		mdelay(40);   //wait for 2frames
		sony_disp_powerdown();
#endif        
		sony_state.disp_powered_up = FALSE;
		sony_state.disp_initialized = FALSE;
	}
	return 0;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
extern void lcdbl_set_current(int val);
extern int f_early_suspend;
#endif
static void lcdc_sony_set_backlight(struct msm_fb_data_type *mfd)
{
	int         bl_level;

	bl_level = (int)mfd->bl_level;
	//printk(KERN_WARNING "%s: bl_level : %d\n", __func__, bl_level);

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02)
	if (bl_level == 0)
	{
		gpio_set_value(S1_GPIO_LCD_BL, 0);
	}
	else
	{
		gpio_set_value(S1_GPIO_LCD_BL, 1);
	}
#else
	if(sony_state.disp_initialized && !f_early_suspend)
		lcdbl_set_current(bl_level);
#endif
}

static int __init sony_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = sony_probe,
	.driver = {
		.name   = "lcdc_sony_wvga",
	},
};

static struct msm_fb_panel_data sony_panel_data = {
	.on = lcdc_sony_panel_on,
	.off = lcdc_sony_panel_off,
	.set_backlight = lcdc_sony_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_sony_wvga",
	.id	= 1,
	.dev	= {
		.platform_data = &sony_panel_data,
	}
};

static int __init lcdc_sony_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &sony_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
	pinfo->bpp = 18;
#else
	pinfo->bpp = 32;
#endif
	pinfo->fb_num = 2;
	/* 30Mhz mdp_lcdc_pclk and mdp_lcdc_pad_pcl */
	pinfo->bl_max = 15;
	pinfo->bl_min = 1;

#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_PT02)
	pinfo->clk_rate = 24820000;
	//pinfo->clk_rate = 24576000;

	pinfo->lcdc.h_back_porch = 20;//+8
	pinfo->lcdc.h_front_porch = 16;
	pinfo->lcdc.h_pulse_width = 4;
    
	pinfo->lcdc.v_back_porch = 4;//+1
	pinfo->lcdc.v_front_porch = 3;
	pinfo->lcdc.v_pulse_width = 2;
    
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff0000;
	pinfo->lcdc.hsync_skew = 0;
#else
	pinfo->clk_rate = 24576000;//24.576MHz

	pinfo->lcdc.h_back_porch = 20;
	pinfo->lcdc.h_front_porch = 20;
	pinfo->lcdc.h_pulse_width = 10;

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
	pinfo->lcdc.v_back_porch = 4;		// real VBP = VBP + 2
	pinfo->lcdc.v_front_porch = 6;		// real VFP = VFP - 2
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
	pinfo->lcdc.v_back_porch = 8;		// real VBP = VBP + 2
	pinfo->lcdc.v_front_porch = 8;		// real VFP = VFP - 2
#endif	
	pinfo->lcdc.v_pulse_width = 2;

	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff0000;
	pinfo->lcdc.hsync_skew = 0;
#endif

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

device_initcall(lcdc_sony_panel_init);

