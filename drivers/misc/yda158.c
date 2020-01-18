/*
    Copyright (C) 2008 Tae June Kim

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/vreg.h>
#include <mach/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include "mach/board-s1.h"


static struct i2c_client *m_pAmpClient;
static int m_nOpenCtr = 0;

static int audio_power(int enable)
{
	struct vreg *vreg_audio;
	int 	rc = 0;

	vreg_audio = vreg_get(NULL, "rftx");
	if (IS_ERR(vreg_audio)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_audio));
		rc = -EIO;
		goto exit;
	}

	if (enable)
	{
		gpio_set_value(S1_GPIO_AUDIO_AMP_nRESET, 0);
		/* units of mV, steps of 50 mV */
		rc = vreg_set_level(vreg_audio, 2800);
		if (rc) {
			printk(KERN_ERR "%s: vreg audio set level failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		rc = vreg_enable(vreg_audio);
		if (rc) {
			printk(KERN_ERR "%s: vreg audio enable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		mdelay(10);
		gpio_set_value(S1_GPIO_AUDIO_AMP_nRESET, 1);
		mdelay(10);
	}
	else
	{
		rc = vreg_set_level(vreg_audio, 0);
		if (rc) {
			printk(KERN_ERR "%s: vreg audio set level failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		rc = vreg_disable(vreg_audio);
		if (rc) {
			printk(KERN_ERR "%s: vreg audio disable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		gpio_set_value(S1_GPIO_AUDIO_AMP_nRESET, 0);
	}
	printk(KERN_DEBUG "Audio power switch: %s\n", enable ? "on" : "off");

exit:
	return (rc);
}


#if 0
static void yda158_work_func(struct work_struct *work)
{
}

DECLARE_WORK(yda158_work, yda158_work_func);

static irqreturn_t yda158_irq(int irq, void *dev_id)
{
/*	
	if (irq == MSM_GPIO_TO_INT(GPIO_PS_INT))
	{
		schedule_work(&yda158_work);
	}
*/	
	return IRQ_HANDLED;
}
#endif


#define	YDA158_MAGIC		'Y'
#define	YDA158_IOCTL_MAX	10
#define YDA158_SET_PATH		_IOW (YDA158_MAGIC, 1, u16)
#define YDA158_SET_REG		_IOW (YDA158_MAGIC, 2, u16)
#define YDA158_GET_REG		_IOWR(YDA158_MAGIC, 3, u16)
#define YDA158_SET_VOL		_IOWR(YDA158_MAGIC, 4, u32)
#define YDA158_STANDBY		_IOW (YDA158_MAGIC, 5, u16)
#define YDA158_SETMUTE		_IOW (YDA158_MAGIC, 6, u16)
#define YDA158_STOP_SUSPEND	_IOW (YDA158_MAGIC, 7, u16)

#define	YDA158_OUT_NONE		0x00
#define YDA158_OUT_HP		0x01
#define	YDA158_OUT_SP		0x02

#define	YDA158_IN_NONE		0x00
#define	YDA158_IN_MIN		0x01
#define	YDA158_IN_1		0x02
#define	YDA158_IN_2		0x04
#define	YDA158_IN_RECV		0x08


#define	YAD158_REG_PWR_DOWN1	0x80
#define	YAD158_REG_PWR_DOWN2	0x81
#define	YAD158_REG_PWR_DOWN3	0x82
#define	YAD158_REG_PWR_DOWN4	0x83
#define YDA158_REG_NON_CLIP2	0x84
#define YDA158_REG_NON_CLIP2_AT	0x85

#define	YAD158_REG_VOL_MIN	0x87
#define	YAD158_REG_VOL_LIN1	0x88
#define	YAD158_REG_VOL_RIN1	0x89
#define	YAD158_REG_VOL_LIN2	0x8a
#define	YAD158_REG_VOL_RIN2	0x8b

#define	YAD158_REG_HP_MIX_L	0x8c
#define	YAD158_REG_HP_MIX_R	0x8d
#define	YAD158_REG_SPK_MIX	0x8e
#define	YAD158_REG_SPK_ATT	0x8f
#define	YAD158_REG_HP_ATT_L	0x90
#define	YAD158_REG_HP_ATT_R	0x91
#define	YAD158_REG_HP_RDY_L	0x93
#define	YAD158_REG_HP_RDY_R	0x94

/* Attenuation -15db  for Headset */
#define YAD158_HP_ATT_DEFAULT	0x1B

#define YAD158_HP_ATT_BOTH_SPK	0x0B

/* Attenuation -4db  for Speaker  */
#define YAD158_SPK_ATT_DEFAULT	27


static u_char			m_ucIn = 0, m_ucOut = 0;
static u_char			m_tblRegs[21];
static u16			m_wStopSuspend = 0;
#define mREG(ucReg)		m_tblRegs[(ucReg) - 0x80]

static int yda158_write_reg(u_char ucReg, u_char ucData)
{
	int rc = 0;
	
	if (ucReg < 0x80 || ucReg > 0x94)
		return (-1);

	rc = i2c_smbus_write_byte_data(m_pAmpClient, ucReg, ucData);

	return (rc);
}

static int yda158_update_reg(u_char ucReg, u_char ucData)
{
	int rc = 0;
	
	if (ucReg < 0x80 || ucReg > 0x94)
		return (-1);

	if (ucData != mREG(ucReg))
	{
		mREG(ucReg) = ucData;
		rc = yda158_write_reg(ucReg, ucData);
	}

	return (rc);
}

static void yda158_set_mixer(u_char ucIn, u_char ucOut)
{
	u_char	ucReg;
	
	ucReg = 0;
	if (ucIn & YDA158_IN_MIN)
	{
		ucReg |= 0x04;
	}
	if (ucIn & YDA158_IN_1)
	{
		ucReg |= 0x02;
	}
	if (ucIn & YDA158_IN_2)
	{
		ucReg |= 0x01;
	}

	if (ucOut & YDA158_OUT_SP)
	{
		yda158_update_reg(YAD158_REG_SPK_MIX, ucReg);
	}
	if (ucOut & YDA158_OUT_HP)
	{
		yda158_update_reg(YAD158_REG_HP_MIX_L, ucReg);
		yda158_update_reg(YAD158_REG_HP_MIX_R, ucReg);
	}
}

static int yda158_enable_input(u_char ucIn)
{
	int	rc;
	u_char	ucReg, ucShunt;

	ucReg = mREG(YAD158_REG_PWR_DOWN2);
	ucReg |= (0x07);

	if (ucIn & YDA158_IN_MIN)
	{
		yda158_write_reg(YAD158_REG_VOL_MIN, 0);
		ucReg &= ~(0x11);	/* cancel PD_VM */
	}
	
	if (ucIn & YDA158_IN_1)
	{
		yda158_write_reg(YAD158_REG_VOL_LIN1, 0);
		yda158_write_reg(YAD158_REG_VOL_RIN1, 0);
		ucReg &= ~(0x12);	/* cancel PD_VA */
	}
	
	if (ucIn & YDA158_IN_2)
	{
		yda158_write_reg(YAD158_REG_VOL_LIN2, 0);
		yda158_write_reg(YAD158_REG_VOL_RIN2, 0);
		ucReg &= ~(0x14);	/* cancel PD_VB */
	}

	/* 3. cancel power down */
	rc = yda158_update_reg(YAD158_REG_PWR_DOWN2, ucReg);
	mdelay(23);

	/* 4. set shunt */
	if (ucIn & YDA158_IN_RECV)
	{
		if ((mREG(YAD158_REG_PWR_DOWN3) & 0x20) == 0)
		{
			/* turn off speaker amp */
			ucReg = mREG(YAD158_REG_PWR_DOWN3) | 0x20;
			yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		}

		/* HI-Z speaker amp when speaker amp is off */
		ucReg = mREG(YAD158_REG_PWR_DOWN3) | 0x40;
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		ucShunt = 0x01;
	}
	else
	{
		if ((mREG(YAD158_REG_PWR_DOWN3) & 0x40) == 0x40)
		{
			/* gound speaker amp when speaker amp is off */
			ucReg = mREG(YAD158_REG_PWR_DOWN3) & ~(0x40);
			yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		}
		ucShunt = 0x03;
	}
	yda158_update_reg(YAD158_REG_PWR_DOWN4, ucShunt);

	/* 5. restore input volume */
	if (ucIn & YDA158_IN_MIN)
	{
		yda158_write_reg(YAD158_REG_VOL_MIN, mREG(YAD158_REG_VOL_MIN));
	}
	if (ucIn & YDA158_IN_1)
	{
		yda158_write_reg(YAD158_REG_VOL_LIN1, mREG(YAD158_REG_VOL_LIN1));
		yda158_write_reg(YAD158_REG_VOL_RIN1, mREG(YAD158_REG_VOL_RIN1));
	}
	if (ucIn & YDA158_IN_2)
	{
		yda158_write_reg(YAD158_REG_VOL_LIN2, mREG(YAD158_REG_VOL_LIN2));
		yda158_write_reg(YAD158_REG_VOL_RIN2, mREG(YAD158_REG_VOL_RIN2));
	}

	return (rc);
}

static int yda158_enable_output(u_char ucOut)
{
	u_char	ucReg, ucReg1;

	ucReg = mREG(YAD158_REG_PWR_DOWN3);
	ucReg |= 0x3b;

	if (ucOut & YDA158_OUT_HP)
	{
		ucReg1 = mREG(YAD158_REG_PWR_DOWN2) & (~0x08);
		yda158_update_reg(YAD158_REG_PWR_DOWN2, ucReg1);	/* cancel PD_REG */
		udelay(500);
		
		ucReg &= ~(0x01);					/* cancel PD_CHP */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		udelay(500);
		
		ucReg &= ~(0x02);					/* cancel PD_HP  */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		mdelay(8);
	}
	else
	{
		ucReg |= 0x02;						/* PD_HP  */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		mdelay(8);

		ucReg |= 0x01;						/* PD_CHP */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
		mdelay(8);

		ucReg1 = mREG(YAD158_REG_PWR_DOWN2) | 0x08;
		yda158_update_reg(YAD158_REG_PWR_DOWN2, ucReg1);	/* PD_REG */
		udelay(500);
	}
	
	if (ucOut & YDA158_OUT_SP)
	{
		ucReg &= ~(0x20);					/* cancel PD_SP  */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);
	}
	else
	{
		ucReg |= 0x20;						/* PD_SP  */
		yda158_update_reg(YAD158_REG_PWR_DOWN3, ucReg);	
	}

	return (0);
}

static void yda158_set_path(u_char ucIn, u_char ucOut)
{
	if (ucIn != YDA158_IN_NONE || ucOut != YDA158_OUT_NONE)
	{
		yda158_update_reg(YAD158_REG_PWR_DOWN1, 0);
	}

	yda158_write_reg(YAD158_REG_HP_ATT_L, 0);	/* 1. mute HP_ATT                     */
	yda158_write_reg(YAD158_REG_HP_ATT_R, 0);
	yda158_write_reg(YAD158_REG_SPK_ATT, 0);	/* 1. mute SP_ATT                     */

	if (ucOut & YDA158_OUT_HP)
	{
		yda158_update_reg(YAD158_REG_HP_MIX_L, 0);	/* 2. mute headphone mixer            */
		yda158_update_reg(YAD158_REG_HP_MIX_R, 0);
		yda158_enable_output(ucOut);			/* 3. cancel power-down of the output */
		yda158_enable_input(ucIn);			/* 4. cancel power-down of the input  */
		yda158_set_mixer(ucIn, ucOut);			/* 5. set mixer                       */
		mdelay(20);
		
		/* 6. unmute HP_ATT                   */
		/* if output is YDA158_OUT_SP either, then volume down */
		if (ucOut & YDA158_OUT_SP)
		{
			mREG(YAD158_REG_HP_ATT_L) = YAD158_HP_ATT_BOTH_SPK;
			mREG(YAD158_REG_HP_ATT_R) = YAD158_HP_ATT_BOTH_SPK;
		}
		else
		{
			mREG(YAD158_REG_HP_ATT_L) = YAD158_HP_ATT_DEFAULT;
			mREG(YAD158_REG_HP_ATT_R) = YAD158_HP_ATT_DEFAULT;		
		}
		yda158_write_reg(YAD158_REG_HP_ATT_L, mREG(YAD158_REG_HP_ATT_L));
		yda158_write_reg(YAD158_REG_HP_ATT_R, mREG(YAD158_REG_HP_ATT_R));
	}

	if (ucOut & YDA158_OUT_SP)
	{
		yda158_update_reg(YAD158_REG_SPK_MIX, 0);	/* 2. mute spk mixer                  */
		yda158_enable_output(ucOut);			/* 3. cancel power-down of the output */
		yda158_enable_input(ucIn);			/* 4. cancel power-down of the input  */
		yda158_set_mixer(ucIn, ucOut);			/* 5. set mixer                       */
		mdelay(20);
		/* 6. unmute SP_ATT                   */
		yda158_write_reg(YAD158_REG_SPK_ATT, mREG(YAD158_REG_SPK_ATT));
	}

	if (ucIn == YDA158_IN_NONE && ucOut == YDA158_OUT_NONE)
	{
		yda158_enable_output(YDA158_OUT_NONE);
		yda158_enable_input(YDA158_IN_NONE);
		yda158_update_reg(YAD158_REG_PWR_DOWN1, 1);	/* enable power-down of the internal clock */
	}
}


static void yda158_set_default_volume(void)
{
	/* Input Volume 0db    = 23 */
	yda158_update_reg(YAD158_REG_VOL_MIN,  23);
	yda158_update_reg(YAD158_REG_VOL_LIN1, 0x1B);
	yda158_update_reg(YAD158_REG_VOL_RIN1, 0x1B);
	yda158_update_reg(YAD158_REG_VOL_LIN2, 0x1B);
	yda158_update_reg(YAD158_REG_VOL_RIN2, 0x1B);

	/* Attenuation -4db  for Speaker  */
	mREG(YAD158_REG_SPK_ATT) = YAD158_SPK_ATT_DEFAULT;

    	/* Attenuation -15db  for Headset */
	mREG(YAD158_REG_HP_ATT_L) = YAD158_HP_ATT_DEFAULT;
	mREG(YAD158_REG_HP_ATT_R) = YAD158_HP_ATT_DEFAULT;

	yda158_update_reg(YDA158_REG_NON_CLIP2, 0x30);
	
	/* release 0.30s/dB, attack 0.12ms/dB */
	//yda158_update_reg(YDA158_REG_NON_CLIP2_AT, 0x0D);
}

static void yda158_set_vol(u32 dwVal)
{
	u8	ucDev;
	u8	ucVol;

	ucDev = (dwVal >> 24) & 0xff;	/* input dev */
	ucVol = dwVal & 0x1f;
	
	if (ucDev & YDA158_IN_MIN)
	{
		yda158_update_reg(YAD158_REG_VOL_MIN, ucVol);
	}
	if (ucDev & YDA158_IN_1)
	{
		yda158_update_reg(YAD158_REG_VOL_LIN1, ucVol);
		yda158_update_reg(YAD158_REG_VOL_RIN1, ucVol);
	}
	if (ucDev & YDA158_IN_2)
	{
		yda158_update_reg(YAD158_REG_VOL_LIN2, ucVol);
		yda158_update_reg(YAD158_REG_VOL_RIN2, ucVol);
	}

	ucDev = (dwVal >> 16) & 0xff;	/* output dev */
	if (ucDev & YDA158_OUT_HP)
	{
		yda158_update_reg(YAD158_REG_HP_ATT_L, ucVol);
		yda158_update_reg(YAD158_REG_HP_ATT_R, ucVol);
	}
	if (ucDev & YDA158_OUT_SP)
	{
		yda158_update_reg(YAD158_REG_SPK_ATT, ucVol);
	}
}

static int yda158_init_read(void)
{
	int 	i, rc = 0;
	
	for ( i = 0; i < ARRAY_SIZE(m_tblRegs); i++)
	{
		rc = i2c_smbus_read_byte_data(m_pAmpClient, 0x80 + i); 
		if (rc < 0)
			return (rc);
		m_tblRegs[i] = (u_char)rc;
	}
	yda158_update_reg(YAD158_REG_PWR_DOWN3, 0x3b);		/* enable HI-Z HP */

	return (0);
}

static int yda158_enable_common(int enable)
{
	int	rc = 0;
	u_char	ucReg;

	if (enable)
	{
		/* cancel power-down of internal clock (PDPC)*/
		ucReg = 0;
		rc |= yda158_update_reg(YAD158_REG_PWR_DOWN1, ucReg);

		/* cancel power-down of common circuit (PDP)*/
		ucReg = mREG(YAD158_REG_PWR_DOWN2) & ~(0x10);
		rc |= yda158_update_reg(YAD158_REG_PWR_DOWN2, ucReg);
		yda158_set_default_volume();
	}
	else
	{
		/* cancel power-down of common circuit (PDP)*/
		ucReg = mREG(YAD158_REG_PWR_DOWN2) | (0x10);
		rc |= yda158_update_reg(YAD158_REG_PWR_DOWN2, ucReg);

		/* cancel power-down of internal clock (PDPC)*/
		ucReg = 1;
		rc |= yda158_update_reg(YAD158_REG_PWR_DOWN1, ucReg);
	}
	
	return (rc);
}

static int yda158_open(struct inode *inode, struct file *filp)
{
	int	rc = 0;

	printk(KERN_DEBUG "yda158_open : %d\n", m_nOpenCtr);
	if (m_nOpenCtr == 0)
	{
		if (audio_power(1))
		{
			rc = -EIO;
			goto exit;
		}
		if (yda158_init_read() < 0)
		{
			rc = -EIO;
			goto exit;
		}
		if (yda158_enable_common(1) < 0)
		{
			rc = -EIO;
			goto exit;
		}
	}
	m_nOpenCtr++;

exit:
	return rc;
}

static int yda158_mute(int enable)
{
	if (enable == 1)
	{
		if (m_ucOut & YDA158_OUT_HP)
		{
			/* 6. mute HP_ATT                   */
			yda158_write_reg(YAD158_REG_HP_ATT_L, 0);
			yda158_write_reg(YAD158_REG_HP_ATT_R, 0);
		}
		if (m_ucOut & YDA158_OUT_SP)
		{
			/* 6. mute SP_ATT                   */
			yda158_write_reg(YAD158_REG_SPK_ATT, 0);
		}
	}
	else
	{
		if (m_ucOut & YDA158_OUT_HP)
		{
			/* 6. unmute HP_ATT                   */
			yda158_write_reg(YAD158_REG_HP_ATT_L, mREG(YAD158_REG_HP_ATT_L));
			yda158_write_reg(YAD158_REG_HP_ATT_R, mREG(YAD158_REG_HP_ATT_L));
		}
		if (m_ucOut & YDA158_OUT_SP)
		{
			/* 6. unmute SP_ATT                   */
			yda158_write_reg(YAD158_REG_SPK_ATT, mREG(YAD158_REG_SPK_ATT));
		}
	}

	return (0);
}

static int yda158_ioctl(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	u16	wPath, wData;
	u32	dwVol;
	
	if (_IOC_TYPE(cmd) != YDA158_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) >= YDA158_IOCTL_MAX)
		return -EINVAL;

	switch (cmd)
	{
		case YDA158_SET_PATH:
			if (get_user(wPath, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			m_ucIn = wPath >> 8;
			m_ucOut = wPath & 0xff;
			printk(KERN_ERR "IN : %d, OUT : %d\n", m_ucIn, m_ucOut);
			yda158_set_path(m_ucIn, m_ucOut);
			break;

		case YDA158_SET_VOL:
			if (get_user(dwVol, (u32 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			
			yda158_set_vol(dwVol);
			break;

		case YDA158_SET_REG:
			if (get_user(wPath, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			yda158_update_reg(wPath >> 8, wPath & 0xff);
			break;

		case YDA158_GET_REG:
			if (get_user(wPath, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			rc = i2c_smbus_read_byte_data(m_pAmpClient, (u8)wPath);
			wData = (wPath & 0xff) << 8 | (rc);
			if (put_user(wData, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			rc = 0;
			break;

		case YDA158_STANDBY:
			if (get_user(wPath, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			printk(KERN_ERR ">>> YDA158_STANDBY : %d\n", wPath);
			if (wPath == 1)
			{
				yda158_set_path(YDA158_IN_NONE, YDA158_OUT_NONE);
			}
			else
			{
				yda158_set_path(m_ucIn, m_ucOut);
			}
			break;

		case YDA158_SETMUTE:
			if (get_user(wPath, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			
			printk(KERN_ERR ">>> YDA158_SETMUTE : %d\n", wPath);
			yda158_mute(wPath);
			break;

		case YDA158_STOP_SUSPEND:
			if (get_user(m_wStopSuspend, (u16 __user *)arg))
			{
				rc = -EFAULT;
				goto exit;
			}
			printk(KERN_ERR ">>> YDA158_STOP_SUSPEND : %d\n", m_wStopSuspend);
			break;

		default:
			break;
	}

exit:	
	return rc;
}

static int yda158_release(struct inode *inode, struct file *filp)
{
	int	rc = 0;

	printk(KERN_DEBUG "yda158_release : %d\n", m_nOpenCtr);
	m_nOpenCtr--;
	if (m_nOpenCtr == 0)
	{
		if (audio_power(0) != 0)
		{
			rc = -EIO;
			goto exit;
		}
	}

exit:	
	return rc;
}

static const struct file_operations yda158_fops = {
	.owner   = THIS_MODULE,
	.open    = yda158_open,
	.ioctl   = yda158_ioctl,
	.release = yda158_release,
};

static struct miscdevice yda158_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "yda158",
	.fops = &yda158_fops,
};


#ifdef CONFIG_PM
static int yda158_pm_suspend(struct device *dev)
{
	if (!m_wStopSuspend)
	{
		if (yda158_enable_common(0))
			printk(KERN_ERR "%s : fail to disable common\n", __func__);
		
		if (audio_power(0))
			printk(KERN_ERR "%s : fail to turn off power\n", __func__);
	}
	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static int yda158_pm_resume(struct device *dev)
{
	if (!m_wStopSuspend)
	{
		if (audio_power(1) < 0)
			printk(KERN_ERR "%s : fail to turn on power\n", __func__);

		if (yda158_init_read() < 0)
			printk(KERN_ERR "%s : fail to read init regs\n", __func__);

		if (yda158_enable_common(1) < 0)
			printk(KERN_ERR "%s : fail to enable common\n", __func__);
	}
	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static struct dev_pm_ops yda158_pm_ops = {
	.suspend = yda158_pm_suspend,
	.resume  = yda158_pm_resume,
};
#endif


static int yda158_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int 	rc = 0;
	u32	gpio_cfg = GPIO_CFG(S1_GPIO_AUDIO_AMP_nRESET, 0, GPIO_OUTPUT, 
			GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_tlmm_config(gpio_cfg, GPIO_ENABLE);
	if (rc)
		goto exit;

	if (audio_power(1))
		goto exit;

	m_pAmpClient = client;

	rc = misc_register(&yda158_misc);
	if (unlikely(rc)) {
		printk(KERN_ERR "YDA158: failed to register misc device!\n");
		goto exit;
	}

	if (audio_power(0))
	{
		rc = -EIO;
		goto exit;
	}

exit:
	return rc;
}


static int yda158_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));

	return 0;
}


static const struct i2c_device_id yda158_id[] = {
	{ "yda158", 0 },
	{ }
};


static struct i2c_driver yda158_driver = {
	.driver = {
		.name  = "yda158",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm    = &yda158_pm_ops,
#endif
	},
	.probe      = yda158_probe,
	.remove     = yda158_remove,
	.id_table   = yda158_id,
	.class      = I2C_CLASS_HWMON,
};

static int __init yda158_init(void)
{
	int ret;

	ret = i2c_add_driver(&yda158_driver);
	printk(KERN_WARNING
		"yda158_init : %d\n", ret);

	return ret;
}

static void __exit yda158_exit(void)
{
	i2c_del_driver(&yda158_driver);
}


MODULE_AUTHOR("TJ Kim");
MODULE_DESCRIPTION("YAMAHA YDA158 driver");
MODULE_LICENSE("GPL");


module_init(yda158_init);
module_exit(yda158_exit);

