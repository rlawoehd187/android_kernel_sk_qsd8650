/*
    Copyright (C) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
			       Philip Edelbrock <phil@netroedge.com>
    Copyright (C) 2003 Greg Kroah-Hartman <greg@kroah.com>
    Copyright (C) 2003 IBM Corp.
    Copyright (C) 2004 Jean Delvare <khali@linux-fr.org>

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

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>
#include <mach/board-s1.h>


#define	LTR_MAGIC	'n'
#define	LTR_IOCTL_MAX	6

typedef struct
{
	unsigned char	addr;
	unsigned char value;
} LTR_REG_IO_T;


#define LTR_SET_REG	_IOW (LTR_MAGIC, 0, LTR_REG_IO_T)

#define LTR_GET_REG	_IOWR (LTR_MAGIC, 1, LTR_REG_IO_T)


typedef struct
{
	unsigned char	ucStatus;
	unsigned short	wData;
} LTR_DATA_T;

#define LTR_GET_DATA	_IOR (LTR_MAGIC, 2, LTR_DATA_T)

#define LIGHT_ON	(1)
#define LIGHT_OFF	(2)
#define PROXI_ON	(4)
#define PROXI_OFF	(8)
#define LTR_SET_POWER	_IOW (LTR_MAGIC, 3, unsigned int)



static struct i2c_client *m_pLTRClient;
static bool proximity_isr_enabled = false;
static int ltr_open_count=0;
static int proxi_pon_count=0;
static int light_pon_count=0;
static struct wake_lock ltr_wake_lock;

#define LTR_CFG_REG		0x00
#define LTR_TIMING_CTL_REG	0x01
#define LTR_ALPS_CTL_REG	0x02
#define LTR_INT_STATUS_REG	0x03
#define LTR_PXY_CTL_REG		0x04
#define LTR_DATA_REG		0x05
#define LTR_ALPS_WINDOW_REG	0x08


#define LTR_MODE_PWR_UP		(0x00 << 2)
#define LTR_MODE_PWR_DOWN	(0x02 << 2)
#define LTR_MODE_SW_RESET	(0x03 << 2)
#define LTR_OPR_ALPS_EN		(0x00 << 0)
#define LTR_OPR_PXY_EN		(0x01 << 0)
#define LTR_OPR_APLS_PXY_EN	(0x02 << 0)
#define LTR_OPR_IDLE		(0x03 << 0)

#define LTR_TIMING_PXY_1	(0x00 << 4)
#define LTR_TIMING_PXY_4	(0x01 << 4)
#define LTR_TIMING_PXY_6	(0x02 << 4)
#define LTR_TIMING_PXY_8	(0x03 << 4)
#define LTR_TIMING_TIME_100MS	(0x00 << 2)
#define LTR_TIMING_TIME_200MS	(0x01 << 2)
#define LTR_TIMING_ALPS_1	(0x00 << 0)
#define LTR_TIMING_ALPS_4	(0x01 << 0)
#define LTR_TIMING_ALPS_8	(0x02 << 0)
#define LTR_TIMING_ALPS_16	(0x03 << 0)



static int ltr_init_dev(void)
{
	s32 rc;

	gpio_set_value(S1_GPIO_ALPS_EN, 1);
	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_CFG_REG, LTR_MODE_PWR_DOWN | LTR_OPR_IDLE);
	if (rc < 0)
	{
		return rc;
	}
	udelay(500);

	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_TIMING_CTL_REG, LTR_TIMING_PXY_1 | LTR_TIMING_TIME_100MS | LTR_TIMING_ALPS_1);
	if (rc < 0)
	{
		return rc;
	}
	udelay(500);

	/* 64 Level (ADC Range), Thresh = 0 */
	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_ALPS_CTL_REG, 0xA0);
	if (rc < 0)
	{
		return rc;
	}
	udelay(500);

	/* PXY Accuracy = 7 PXY Counts, Thread Hold = '8'<=='A' HW team req 20101028  */
	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_PXY_CTL_REG, 0x48);
	if (rc < 0)
	{
		return rc;
	}
	udelay(500);

	/* Window Loss Comp = 0 */
	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_ALPS_WINDOW_REG, 0x00);
	if (rc < 0)
	{
		return rc;
	}
	udelay(500);

	return (rc);
}

static int ltr_enable(int proximity, int alps)
{
	u8 	value;
	s32	rc;

	if (proximity && alps)
	{
		value = LTR_MODE_PWR_UP | LTR_OPR_APLS_PXY_EN;
	}
	else if (proximity)
	{
		value = LTR_MODE_PWR_UP | LTR_OPR_PXY_EN;
	}
	else if (alps)
	{
		value = LTR_MODE_PWR_UP | LTR_OPR_ALPS_EN;
	}
	else
	{
		value = LTR_MODE_PWR_DOWN | LTR_OPR_IDLE;
	}
	rc = i2c_smbus_write_byte_data(m_pLTRClient, LTR_CFG_REG, value);
	udelay(500); 

	if (proximity)
	{
		if (!proximity_isr_enabled)
		{
			proximity_isr_enabled = true;
			enable_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT));
			set_irq_wake(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT), 1);
		}
	}
	else
	{
		if (proximity_isr_enabled)
		{
			proximity_isr_enabled = false;
			set_irq_wake(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT), 0);
			disable_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT));
		}
	}

	//printk(KERN_WARNING	"ltr_enable : proximity_isr_enabled=%d\n", proximity_isr_enabled);	

/*
	if (!proximity && !alps)
	{
		gpio_set_value(S1_GPIO_ALPS_EN, 0);
	}
*/
	return (rc);
}

static void ltr_work_func(struct work_struct *work)
{
	wake_lock_timeout(&ltr_wake_lock, 3 * HZ);
#if 0
	u8	uSts;
//	s32	lData;

	uSts  = i2c_smbus_read_byte_data(m_pLTRClient, LTR_INT_STATUS_REG); 
	if (uSts & 0x01)	/* ALPS Int */
	{
	}
	if (uSts & 0x02)	/* Proximity Int */
	{
	}

//	lData = i2c_smbus_read_byte_data(m_pLTRClient, LTR_DATA_REG);
#endif
}
DECLARE_WORK(ltr_work, ltr_work_func);

static irqreturn_t ltr_irq(int irq, void *dev_id)
{
 	if (irq == MSM_GPIO_TO_INT(S1_GPIO_PS_nINT))
 	{
		schedule_work(&ltr_work);
	}

 	return IRQ_HANDLED;
}

static int ltr_read_sts_data(u8 *status, u8 *data)
{
	s32	rc;

	rc = i2c_smbus_read_byte_data(m_pLTRClient, LTR_INT_STATUS_REG);
	if (rc < 0)
	{
		return rc;
	}
	*status = (u8)rc;

	rc = i2c_smbus_read_byte_data(m_pLTRClient, LTR_DATA_REG); 
	if (rc < 0)
	{
		return rc;
	}
	*data = (u8)rc;

	return (rc);
}

static int ltr_open(struct inode *inode, struct file *filp)
{
	if (++ltr_open_count == 1) ltr_init_dev();	

	//printk(KERN_WARNING	"ltr_open : ltr_open_count=%d\n", ltr_open_count);	

	return 0;
}

static ssize_t ltr_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	u8	ucSts;
	u8	ucData;
	u32	dwVal;
	int	rc;

	rc = ltr_read_sts_data(&ucSts, &ucData);
	if (rc < 0)
	{
		return -EIO;
	}

	dwVal = (ucSts << 16) | ucData;
	if (put_user(dwVal, (u32 __user *)buf))
	{
		return -EFAULT;
	}

	return sizeof(u32);
}

static ssize_t ltr_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	u32	dwVal;

	if (get_user(dwVal, (u32 __user *)buf))
	{
		return  -EFAULT;
	}

	return sizeof(u32);
}

static int ltr_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;

	switch (cmd)
	{
		case LTR_SET_REG:
		{
			LTR_REG_IO_T regIO;
			s32 rc;

			if (copy_from_user(&regIO, (LTR_REG_IO_T *)arg, sizeof(regIO)))
				return -EFAULT;

			rc = i2c_smbus_write_byte_data(m_pLTRClient, regIO.addr, regIO.value);
			if (rc < 0)
			{
				return rc;
			}
			udelay(500);

		}
		break;

		case LTR_GET_REG:
		{
			LTR_REG_IO_T regIO;
			s32 rc; 

			if (copy_from_user(&regIO, (LTR_REG_IO_T *)arg, sizeof(regIO)))
				return -EFAULT;

			regIO.value = i2c_smbus_read_byte_data(m_pLTRClient, regIO.addr);

			if (copy_to_user((LTR_REG_IO_T *)arg, &regIO, sizeof(regIO)))
			{
				rc = -EFAULT;
			}

		}
		break;

		case LTR_GET_DATA:
		{
			LTR_DATA_T	sData;
			int		rc;
			
			rc = ltr_read_sts_data(&sData.ucStatus, (u8*)&sData.wData);
			if (rc < 0)
			{
				return -EIO;
			}

			//printk(KERN_WARNING	"ltr_ioctl: LTR_GET_DATA : sData.wData=%d\n", sData.wData);
			
			if (copy_to_user((LTR_DATA_T*)arg, &sData, sizeof(sData)))
			{
				rc = -EFAULT;
			}
		}
		break;

		case LTR_SET_POWER:
		{
			u32	dwVal;

			dwVal = (unsigned int)arg;
			
			if (dwVal == LIGHT_ON)
			{				
				if (++light_pon_count > 1) break;				
				else if (proxi_pon_count > 0) ltr_enable(1, 1);
				else ltr_enable(0, 1);
			}
			else if (dwVal == LIGHT_OFF)
			{
				if (--light_pon_count > 0) break;
				else if (proxi_pon_count > 0) ltr_enable(1, 0);
				else ltr_enable(0, 0);
			}
			else if (dwVal == PROXI_ON)
			{				
				if (++proxi_pon_count > 1) break;				
				else if (light_pon_count > 0) ltr_enable(1, 1);
				else ltr_enable(1, 0);
			}
			else if (dwVal == PROXI_OFF)
			{
				if (--proxi_pon_count > 0) break;
				else if (light_pon_count > 0) ltr_enable(0, 1);
				else ltr_enable(0, 0);
			}
			else return -EFAULT;

			//printk(KERN_WARNING	"ltr_ioctl: LTR_SET_POWER : dwVal=%d, light_pon_count=%d, proxi_pon_count=%d \n", 
			//	dwVal, light_pon_count, proxi_pon_count);
			
		}
		break;

		default:
		{
			rc = -EFAULT;
		}
		break;
	}

	return (rc);
}

static int ltr_release(struct inode *inode, struct file *filp)
{		
	if (--ltr_open_count < 1 )
	{
		ltr_enable(0, 0);
		
		ltr_open_count = 0;
		proxi_pon_count = 0;
		light_pon_count = 0;
	}

	//printk(KERN_WARNING	"ltr_release : ltr_open_count=%d\n", ltr_open_count);

	return 0;
}

static const struct file_operations ltr_fops = {
	.owner   = THIS_MODULE,
	.open    = ltr_open,
	.read    = ltr_read,
	.write   = ltr_write,
	.ioctl   = ltr_ioctl,
	.release = ltr_release,
};

static struct miscdevice ltr_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ltr-505",
	.fops = &ltr_fops,
};


static int ltr_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;
	u32 gpio_int_cfg = GPIO_CFG(S1_GPIO_PS_nINT, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	u32 gpio_pwr_cfg = GPIO_CFG(S1_GPIO_ALPS_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_tlmm_config(gpio_int_cfg, GPIO_ENABLE);
	if (rc)
		goto exit;

	rc = gpio_tlmm_config(gpio_pwr_cfg, GPIO_ENABLE);
	if (rc)
		goto exit;

	m_pLTRClient = client;

	rc = ltr_init_dev();
	if(rc < 0)
	{
		printk(KERN_ERR "ltr: failed to init device!\n");
		goto exit;
	}

 	rc = request_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT),
 			 &ltr_irq,
 			 IRQF_TRIGGER_FALLING,
 			 "ltr_irq",
 			 NULL);
 	if (rc)
 		goto exit;
	else
		disable_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT));

	wake_lock_init(&ltr_wake_lock, WAKE_LOCK_SUSPEND, "ltr_sensor");

	rc = misc_register(&ltr_misc);
	if (unlikely(rc)) {
		free_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT), NULL);
		printk(KERN_ERR "ltr: failed to register misc device!\n");
		goto exit;
	}

	return (0);

exit:
	return rc;
}

static int ltr_detect(struct i2c_client *client, int kind,
			 struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (!(adapter->class & I2C_CLASS_HWMON) && client->addr != 0x56)
		return -ENODEV;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)
	 && !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return -ENODEV;

	strlcpy(info->type, "ltr", I2C_NAME_SIZE);

	return 0;
}


static int ltr_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	free_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT), NULL);
	misc_deregister(&ltr_misc);
	wake_lock_destroy(&ltr_wake_lock);

	return 0;
}

static const struct i2c_device_id ltr_id[] = {
	{ "ltr-505", 0 },
	{ }
};


static struct i2c_driver ltr_driver = {
	.driver = {
		.name	= "ltr-505",
	},
	.probe	   = ltr_probe,
	.remove	   = ltr_remove,
	.detect	   = ltr_detect,
	.id_table  = ltr_id,
	.class	  = I2C_CLASS_HWMON,
};

static int __init ltr_init(void)
{
	int ret;

	ret = i2c_add_driver(&ltr_driver);
	printk(KERN_WARNING	"ltr_init : %d\n", ret);	

	return ret;
}

static void __exit ltr_exit(void)
{
	i2c_del_driver(&ltr_driver);
}

MODULE_AUTHOR("TJ Kim");
MODULE_DESCRIPTION("LTR-505ALS Driver");
MODULE_LICENSE("GPL");

module_init(ltr_init);
module_exit(ltr_exit);
