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
#include <mach/board-s1.h>

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
/* type definition */
typedef unsigned char		  byte;
typedef unsigned short		  word;
typedef unsigned long int 	  dword;
typedef unsigned char         boolean;

#define SPIF                  S1_GPIO_LCD_BL

#define HIGH                  1
#define LOW                   0

#define SPIF_MAX_DATA_VALUE   0x3F

/* Register Map */
#define SPIF_BLEN_REG         0x0 //Backlight Enable
#define SPIF_MAIN_BL_REG      0x1 //Main Backlight Current
#define SPIF_SUB_BL_REG       0x2 //Sub Backlight Current
#define SPIF_THIRD_BL_REG     0x3 //Thired Backlight Current
#define SPIF_MAIN_FAID_REG    0x4 //Main Fade
#define SPIF_BL_GROUP_REG     0x5 //Backlight Grouping Configuration

#define TRUE                  1
#define FALSE                 0

static const int bl_value[16] = {
	0x00, //off
	0x07, //2.0mA
	0x09, //3.0mA
	0x0B, //4.0mA [MIN]
	0x0D, //5.0mA
	0x0E, //6.0mA 
	0x0F, //7.0mA
	0x10, //8.0mA
	0x11, //9.0mA
	0x12, //10mA  [MED]
	0x13, //11mA
	0x14, //12mA
	0x15, //13mA
	0x16, //14mA
	0x17, //15mA
	0x18  //16mA  [MAX]
};

static int prev_bl_value = 0;

static void semtech_spif_write(byte addr, byte data)
{
	int nIndex;
	unsigned long flags;

	if(addr > SPIF_BL_GROUP_REG)
	{
		printk(KERN_WARNING "%s: wrong address(0x%x)\n", __func__, addr);
		return;
	}

	if(data > SPIF_MAX_DATA_VALUE)
	{
		printk(KERN_WARNING "%s: data value is exceeded 0x3F(0x%x)\n", __func__, data);
		return;
	}

	/* Send Address */
	//start
	gpio_set_value(SPIF, HIGH);
	mdelay(2);

	/* Time Critical Area, disable all interrupt */
	/* This Area is excuted in 1ms               */
	local_irq_save(flags);
	local_irq_disable();

	/* write address */
	for(nIndex = 0; nIndex < addr + 1; nIndex++)
	{
		gpio_set_value(SPIF, LOW);
		udelay(1);
		gpio_set_value(SPIF, HIGH);
		udelay(1);
	}
	udelay(550);	/* hold_a */

	/* Send Data */
	for(nIndex = 0; nIndex < data + 1; nIndex++)
	{
		gpio_set_value(SPIF, LOW);
		udelay(1);
		gpio_set_value(SPIF, HIGH);
		udelay(1);
	}

	local_irq_restore(flags);
	
	mdelay(15);	/* hold_d + standby */
}

static int get_bl_value(int level)
{
	return bl_value[level];
}

static void lcdbl_leave_shutdown_mode(void)
{
	int	rc;
	u32	cfgSPIF = GPIO_CFG(S1_GPIO_LCD_BL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_tlmm_config(cfgSPIF, GPIO_ENABLE);
	printk(KERN_INFO "%s\n", __func__);
	
	gpio_set_value(S1_GPIO_LCD_BL, HIGH);
	semtech_spif_write(SPIF_BL_GROUP_REG, 0x0);
	semtech_spif_write(SPIF_BLEN_REG, 0x3F);
	semtech_spif_write(SPIF_MAIN_FAID_REG, 0x1);
}

static void lcdbl_enter_shutdown_mode(void)
{
	printk(KERN_INFO "%s\n", __func__);
	gpio_set_value(S1_GPIO_LCD_BL, LOW);
	mdelay(15);
}

void lcdbl_set_current(int level)
{
	int val;
	
	val = get_bl_value(level);
	
	if(val == prev_bl_value)
		return;

	printk(KERN_INFO "%s] level : %d , value : 0x%x \n", __func__, level, val);
	if (val > 0)
	{
		if (prev_bl_value == 0)
		{
			lcdbl_leave_shutdown_mode();
		}
		semtech_spif_write(SPIF_MAIN_BL_REG, (byte)val);
	}
	else if (val == 0 && prev_bl_value != 0)
	{
		lcdbl_enter_shutdown_mode();
	}
	
	prev_bl_value = val;
}

#endif
