/*
 * drivers/input/touchscreen/tma340.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tma340.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>

#include <linux/proc_fs.h>
#include <linux/hrtimer.h>

#include <linux/time.h>

#define RELEASE_DELAY         (300 * 1000 * 1000) //300 msec
#define BORDERLINE_FOR_TIMER  600

#define TMA340_DEBUG_STRING   6

static struct workqueue_struct *tma340_wq;

static struct i2c_client *tma340_client;
static int enable_debug = 0;
static bool thres_high = 0;

struct tma340 {
	struct input_dev	*input;
	char			phys[32];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend    early_suspend;
#endif
	struct work_struct      work;
	struct i2c_client	*client;
	struct hrtimer		timer;
	spinlock_t		lock;

	int                     touchkey;
};

static struct cyttsp_xy_coordinate_t touch_1st;
static struct cyttsp_xy_coordinate_t touch_2nd;

static int tma340_enable_debug(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char buf[TMA340_DEBUG_STRING];
	int ret = 0;
	static u8 is_called = 0;

	if(copy_from_user(&buf, buffer, TMA340_DEBUG_STRING))
	{
		ret = -EFAULT;
		goto tma340_write_proc_failed;
	}

	if(strncmp(buf, "true", strlen("true")) == 0)
		enable_debug = 1;
	else if(strncmp(buf, "false", strlen("false")) == 0)
		enable_debug = 0;
	else if( (strncmp(buf, "intr", strlen("intr")) == 0) && (is_called==0) )
	{
		printk(KERN_ERR "%s: enable INTR forcely\n",__func__);
		enable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
		is_called = 1;
	}

tma340_write_proc_failed:
	return ret;
}


static int tma340_power(int onoff)
{
	int rc=0;

	struct vreg  *vreg_tma340;
	vreg_tma340 = vreg_get(NULL, "wlan");
	if (IS_ERR(vreg_tma340)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_tma340));
		goto exit;
	}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	gpio_set_value(S1_GPIO_5V_TOUCH_EN, onoff);
#endif

	mdelay(2);

	if (onoff)
	{
		rc = vreg_set_level(vreg_tma340, 2800);
		if (rc)
		{
			printk(KERN_ERR "%s: vreg tma340 set level failed (%d)\n", __func__, rc);
			goto exit;
		}
		rc = vreg_enable(vreg_tma340);
		if (rc) {
			printk(KERN_ERR "%s: vreg tma340 enable failed (%d)\n", __func__, rc);
			goto exit;
		}
	}
	else
	{
		rc = vreg_disable(vreg_tma340);
	}

exit:

	return (rc);
}

static u8 key_touched = 0;
static u8 screen_touched = 0;
static u8 border_area = 0;
static u8 dont_release_key = 0;
static u8 timer_running = 0;
static void tma340_ts_work_func(struct work_struct *work)
{
	uint16_t x1, y1, z1, x2, y2, z2;
	u8 num_of_touches = 0;
	u8 truetouch_stat, int_flags;
	s32 retval;
	u8 i;

	struct tma340 *ts = container_of(work, struct tma340, work);

	disable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));

	if(enable_debug)
		printk(KERN_INFO "%s interrupt occurred\n",__func__);


	i = CY_NUM_RETRY;
	do {
		int_flags = i2c_smbus_read_byte_data(ts->client, TMA340_TOUCH_BTN_MASK);
	}while((int_flags<0) && --i);

	if (int_flags < 0) {
		/* return immediately on
		 * failure to read device on the i2c bus */
		if(enable_debug)
			printk(KERN_INFO "%s I2C read fail\n",__func__);
		goto i2creadfail;
	}

	switch(INTERRUPT_FLAGS(int_flags)) {
	case 1: //key
		if(dont_release_key)
			break;

		if(screen_touched)
		{
			screen_touched = 0;
			if(enable_debug)
				printk(KERN_INFO "%s touches released before key\n", __func__);
			input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 0);
			input_mt_sync(ts->input);
			input_sync(ts->input);
		}

		if (key_touched)
			break;

		key_touched = 1;

		if(border_area)
		{
			if(enable_debug)
				printk(KERN_INFO "%s You scrolled screen without touch-release\n",__func__);
			dont_release_key = 1;
			key_touched = 0;
			break;
		}

		switch(TOUCHED_KEYINFO(int_flags)) {
		case 1:
			ts->touchkey = KEY_MENU;
			break;

		case 2:
			ts->touchkey = KEY_HOME;
			break;

		case 4:
			ts->touchkey = KEY_SEARCH;
			break;

		case 8:
			ts->touchkey = KEY_BACK;
			break;

		default:
			goto i2creadfail;
			break;
		}
		if(enable_debug)
			printk(KERN_INFO "%s touched key : %d\n",__func__, TOUCHED_KEYINFO(int_flags));
		input_report_key(ts->input, ts->touchkey, 1);
		break;

	case 2: //touch
	case 3:
		i = CY_NUM_RETRY;
		do {
			truetouch_stat = i2c_smbus_read_byte_data(tma340_client, TMA340_TRUETOUCH_STAT);
		} while ((truetouch_stat < 0) && --i);

		if (truetouch_stat < 0) {
			/* return immediately on
			 * failure to read device on the i2c bus */
			if(enable_debug)
				printk(KERN_ERR "%s I2C read fail\n",__func__);
			goto i2creadfail;
		}

		if (IS_LARGE_AREA(truetouch_stat))
		{
			if(enable_debug)
				printk(KERN_ERR "%s Large object detected\n",__func__);
			goto i2creadfail;
		}


		if(key_touched)
		{
			key_touched = 0;
			if(enable_debug)
				printk(KERN_INFO "%s key released before screentouch, %d\n", __func__, ts->touchkey);
			input_report_key(ts->input, ts->touchkey, 0);
		}
		screen_touched = 1;

		num_of_touches = GET_NUM_TOUCHES(truetouch_stat);
		switch(num_of_touches)
		{
		case 2:
			i = CY_NUM_RETRY;
			do {
				retval = i2c_smbus_read_i2c_block_data(ts->client, TMA340_2NDTOUCH_BASE, sizeof(struct cyttsp_xy_coordinate_t), (u8 *)&touch_2nd);
			} while ((retval < 0) && --i);
			x2 = be16_to_cpu(touch_2nd.x);
			y2 = be16_to_cpu(touch_2nd.y);
			z2 = touch_2nd.z;
			if (retval < 0) {
				/* return immediately on
				 * failure to read device on the i2c bus */
				if(enable_debug)
					printk(KERN_INFO "%s I2C read fail\n",__func__);
				goto i2creadfail;
			}
		case 1:
			i = CY_NUM_RETRY;
			do {
				retval = i2c_smbus_read_i2c_block_data(ts->client, TMA340_1STTOUCH_BASE, sizeof(struct cyttsp_xy_coordinate_t), (u8 *)&touch_1st);
			} while ((retval < 0) && --i);
			x1 = be16_to_cpu(touch_1st.x);
			y1 = be16_to_cpu(touch_1st.y);
			z1 = touch_1st.z;
			if (retval < 0) {
				/* return immediately on
				 * failure to read device on the i2c bus */
				if(enable_debug)
					printk(KERN_ERR "%s I2C read fail\n",__func__);
				goto i2creadfail;
			}
			break;
		}

		if(num_of_touches!= 0)
		{
			if(enable_debug)
				printk(KERN_INFO "%s touches : %d , x1 : %d , y1 : %d\n", __func__, num_of_touches, x1, y1);

			input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, z1);
			input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 10);
			input_report_abs(ts->input, ABS_MT_POSITION_X, x1);
			input_report_abs(ts->input, ABS_MT_POSITION_Y, y1);
			input_mt_sync(ts->input);

			if (num_of_touches > 1)
			{
				if(enable_debug)
					printk(KERN_INFO "%s touches : %d, x1 : %d, y1 : %d, x2 : %d , y2 : %d\n", __func__, num_of_touches, x1, y1, x2, y2);

				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, z2);
				input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 10);
				input_report_abs(ts->input, ABS_MT_POSITION_X, x2);
				input_report_abs(ts->input, ABS_MT_POSITION_Y, y2);
				input_mt_sync(ts->input);
			}
			input_sync(ts->input);
		}
		if( (y1 > BORDERLINE_FOR_TIMER) && !timer_running )
		{
			if(enable_debug)
				printk(KERN_INFO "%s BORDER LINE Touched\n",__func__);
			hrtimer_start(&ts->timer, ktime_set(0, RELEASE_DELAY), HRTIMER_MODE_REL);
			border_area = 1;
			timer_running = 1;
		}
		break;
	case 0: //release
		if(key_touched)
		{
			key_touched = 0;
			if(enable_debug)
				printk(KERN_INFO "%s key released, %d\n", __func__, ts->touchkey);
			input_report_key(ts->input, ts->touchkey, 0);
			dont_release_key = 0;
		}
		else if (screen_touched)
		{
			screen_touched = 0;
			if(enable_debug)
				printk(KERN_INFO "%s touches released\n", __func__);
			input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 0);
			input_mt_sync(ts->input);
			input_sync(ts->input);
		}
		else if (!key_touched && !screen_touched && dont_release_key)
		{
			dont_release_key = 0;
			if(enable_debug)
				printk(KERN_INFO "%s Reset dont_release_key flag\n", __func__);
		}
		break;

	default:
		goto i2creadfail;
		break;
	}

i2creadfail:
	enable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
}

void tma340_update_threshold(bool flag)
{
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	int ret = 0;
	thres_high = flag;
	if(flag)
	{
		printk(KERN_INFO "%s : for TTA charger\n", __func__);
		ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, 0x02);
		if (ret < 0)
			printk(KERN_ERR "i2 write error : %d\n", ret);
	}
	else
	{
		printk(KERN_INFO "%s : for non charger\n", __func__);
		ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, 0x04);
		if (ret < 0)
			printk(KERN_ERR "i2 write error : %d\n", ret);
	}
#endif
}
EXPORT_SYMBOL(tma340_update_threshold);

static enum hrtimer_restart tma340_timer(struct hrtimer *handle)
{
	if(enable_debug)
		printk(KERN_INFO "%s Clear BORDER_AREA_TOUCHED flag\n",__func__);

	border_area = 0;

	timer_running = 0;
	return HRTIMER_NORESTART;
}


static irqreturn_t tma340_irq(int irq, void *handle)
{
	struct tma340 *ts = handle;

	queue_work(tma340_wq, &ts->work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tma340_early_suspend(struct early_suspend *h)
{
	struct tma340 *ts;

	ts = container_of(h, struct tma340, early_suspend);
	disable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));

	cancel_work_sync(&ts->work);

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	gpio_set_value(S1_GPIO_TOUCHPAD_nRESET, 0);
	tma340_power(0);
#endif

	printk(KERN_INFO "%s\n", __func__);
}

static void tma340_late_resume(struct early_suspend *h)
{
	struct tma340 *ts;
	int ret = 0;
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
	unsigned sclk_cfg = GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
	unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);

	ret = gpio_tlmm_config(sclk_cfg, GPIO_ENABLE);
	if (ret)
	{
		printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
		return;
	}

	ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
	if (ret)
	{
		printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
		return;
	}
#endif
	ts = container_of(h, struct tma340, early_suspend);

	gpio_set_value(S1_GPIO_TOUCHPAD_nRESET, 0);
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
	tma340_power(1);
#endif
	mdelay(5);
	gpio_set_value(S1_GPIO_TOUCHPAD_nRESET, 1);

	enable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
	mdelay(50);
	if(thres_high)
	{
		printk(KERN_INFO "%s : for TTA charger\n", __func__);
		ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, 0x02);
		if (ret < 0)
			printk(KERN_ERR "i2 write error : %d\n", ret);
	}
	else
	{
		printk(KERN_INFO "%s : for non charger\n", __func__);
		ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, 0x04);
		if (ret < 0)
			printk(KERN_ERR "i2 write error : %d\n", ret);
	}
#endif
	printk(KERN_INFO "%s\n", __func__);
}
#endif

static int tma340_open(struct inode *inode, struct file *file)
{
	disable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
	return nonseekable_open(inode, file);
}

static int tma340_release(struct inode *inode, struct file *file)
{
	enable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
	return 0;
}

static int tma340_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

	void __user *argp = (void __user *)arg;

	short buffer;
	int fw_rev;
	unsigned int wait_time;
	int ret = 0;

	if (cmd == ISSP_IOCTL_WAIT)
	{
		if (copy_from_user(&wait_time, argp, sizeof(wait_time)))
			return -EFAULT;
	}
	else if(cmd == ISSP_IOCTL_READ_FW)
	{
		if (copy_from_user(&fw_rev, argp, sizeof(fw_rev)))
			return -EFAULT;
	}
	else
	{
		if (copy_from_user(&buffer, argp, sizeof(buffer)))
			return -EFAULT;
	}

	switch (cmd) {
	case ISSP_IOCTL_SCLK_TO_GPIO:
		if(buffer)
		{
			//unsigned sclk_cfg = GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
			//ret = gpio_tlmm_config(sclk_cfg, GPIO_ENABLE);
			ret = gpio_direction_output(S1_GPIO_TOUCH_SCL, 0);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			//printk(KERN_ERR "tma340_ioctl: SCLK to out\n");
		}
		else
		{
			//unsigned sclk_cfg = GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
			//ret = gpio_tlmm_config(sclk_cfg, GPIO_ENABLE);
			ret = gpio_direction_input(S1_GPIO_TOUCH_SCL);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			//printk(KERN_ERR "tma340_ioctl: SCLK to in\n");
		}
		break;

	case ISSP_IOCTL_DATA_TO_GPIO:
		if(buffer)
		{
			//unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
			//ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
			ret = gpio_direction_output(S1_GPIO_TOUCH_SDA, 0);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			//printk(KERN_ERR "tma340_ioctl: DATA to out\n");
		}
		else
		{
			//unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);
			//ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
			ret = gpio_direction_input(S1_GPIO_TOUCH_SDA);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			//printk(KERN_ERR "tma340_ioctl: DATA to in\n");
		}
		break;

	case ISSP_IOCTL_SCLK:
		//printk(KERN_ERR "tma340_ioctl: SCLK : %d\n", buffer);
		if(buffer)
			gpio_set_value(S1_GPIO_TOUCH_SCL, 1);
		else
			gpio_set_value(S1_GPIO_TOUCH_SCL, 0);
		break;

	case ISSP_IOCTL_DATA:
		//printk(KERN_ERR "tma340_ioctl: DATA : %d\n", buffer);
		if(buffer)
			gpio_set_value(S1_GPIO_TOUCH_SDA, 1);
		else
			gpio_set_value(S1_GPIO_TOUCH_SDA, 0);
		break;

	case ISSP_IOCTL_POWER:
		//printk(KERN_ERR "tma340_ioctl: POWER : %d\n", buffer);
		if(buffer)
			tma340_power(1);
		else
			tma340_power(0);
		break;

	case ISSP_IOCTL_READ_DATA_PIN:
		{
			//unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
			//ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
			//if (ret)
			//{
			//	printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
			//	return ret;
			//}
			buffer = (short)gpio_get_value(S1_GPIO_TOUCH_SDA);
			//printk(KERN_ERR "tma340_ioctl: read value : %d\n", buffer);
		}
		if (copy_to_user(argp, &buffer, sizeof(buffer)))
			return -EFAULT;
		break;

	case ISSP_IOCTL_WAIT:
		udelay(wait_time);
		break;

	case ISSP_IOCTL_RESET:
		if(buffer)
		{
			gpio_set_value(S1_GPIO_TOUCHPAD_nRESET, 1);
		}
		else
		{
			gpio_set_value(S1_GPIO_TOUCHPAD_nRESET, 0);
		}
		break;

	case ISSP_IOCTL_INTR:
		if(buffer)
		{
			printk(KERN_ERR "tma340_ioctl: enable intr\n");
			enable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
		}
		else
		{
			printk(KERN_ERR "tma340_ioctl: disable intr\n");
			disable_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
		}
		break;

	case ISSP_IOCTL_READ_FW:
		{
			fw_rev = i2c_smbus_read_byte_data(tma340_client, TMA340_VERSION_INFO);
			printk(KERN_INFO "%s read FW : %d\n", __func__, fw_rev);
		}
		if (copy_to_user(argp, &fw_rev, sizeof(fw_rev)))
			return -EFAULT;
		break;

	case ISSP_IOCTL_CHANGE_PIN_MODE:
		if(buffer)
		{
			int ret = 0;
			unsigned sclk_cfg = GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
			unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);

			ret = gpio_tlmm_config(sclk_cfg, GPIO_ENABLE);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}

			ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			printk(KERN_ERR "%s TOUCH I2C PINS TO NO-PULL\n",__func__);
		}
		else
		{
			int ret = 0;
			unsigned sclk_cfg = GPIO_CFG(S1_GPIO_TOUCH_SCL, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA);
			unsigned data_cfg = GPIO_CFG(S1_GPIO_TOUCH_SDA, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA);

			ret = gpio_tlmm_config(sclk_cfg, GPIO_ENABLE);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}

			ret = gpio_tlmm_config(data_cfg, GPIO_ENABLE);
			if (ret)
			{
				printk(KERN_ERR "gpio_tlmm_config error : %d\n", ret);
				return ret;
			}
			printk(KERN_ERR "%s TOUCH I2C PINS TO PULL-UP\n",__func__);
		}
		break;

	case ISSP_IOCTL_EXTRA_FUNCTION:
		if(buffer == 0x01) //iDAC calibration
		{
			printk(KERN_ERR "iDAC calibration\n");
			ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, buffer);
			if (ret < 0)
				printk(KERN_ERR "iDAC calibration error : %d\n", ret);
		}
		else if(buffer == 0x02) //adjust Noisethreshold to 50
		{
			printk(KERN_ERR "Noise threshold to 50\n");
			ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, buffer);
			if (ret < 0)
				printk(KERN_ERR "i2 write error : %d\n", ret);
		}
		else if(buffer == 0x04) //adjust Noisethreshold to original
		{
			printk(KERN_ERR "Noise threshold to original\n");
			ret = i2c_smbus_write_byte_data(tma340_client, TMA340_EXTRA_FUNCTION, buffer);
			if (ret < 0)
				printk(KERN_ERR "i2 write error : %d\n", ret);
		}
		break;

	default:
		break;
	}

	return ret;
}

static struct file_operations tma340_fops = {
	.owner = THIS_MODULE,
	.open = tma340_open,
	.release = tma340_release,
	.ioctl = tma340_ioctl,
};

static struct miscdevice tma340_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tma340",
	.fops = &tma340_fops,
};

static int tma340_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tma340 *ts;
	struct tma340_platform_data *pdata = pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err;

	struct proc_dir_entry *tma340_entry;

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	tma340_wq = create_singlethread_workqueue("tma340_wq");
	if (!tma340_wq) {
		err = -ENOMEM;
		goto fail;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "tma340_probe: need I2C_FUNC_I2C\n");
		err = -ENODEV;
	}

	ts = kzalloc(sizeof(struct tma340), GFP_KERNEL);

	INIT_WORK(&ts->work, tma340_ts_work_func);
	
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->input = input_dev;

	hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->timer.function = tma340_timer;

	spin_lock_init(&ts->lock);

	if(tma340_power(1) !=0)
	{
		err = -EIO;
		goto err_free_mem;
	}

	mdelay(5);

	err = pdata->init_platform_hw();
	if (err != 0)
		goto err_free_mem;

	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "s1_touchscr";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_SYN);

	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);
	set_bit(KEY_HOME, input_dev->keybit);
	set_bit(KEY_SEARCH, input_dev->keybit);
	set_bit(KEY_BACK, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, 480, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, 480, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	tma340_client = ts->client;
	err = request_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT), tma340_irq, IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			client->dev.driver->name, ts);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));
		goto err_free_mem;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	//ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	ts->early_suspend.suspend = tma340_early_suspend;
	ts->early_suspend.resume = tma340_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	err = misc_register(&tma340_device);
	if (err) {
		printk(KERN_ERR "tma340_probe: failed to register misc device\n");
		goto err_free_irq;
	}

	dev_info(&client->dev, "registered with irq (%d)\n", MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT));

	tma340_entry = create_proc_entry("tma340_debug",
			S_IRUGO | S_IWUSR | S_IWGRP, NULL);
	if (tma340_entry) {
		tma340_entry->read_proc = NULL;
		tma340_entry->write_proc = tma340_enable_debug;
		tma340_entry->data = NULL;
	}
	
	return 0;

 err_free_irq:
	free_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT), ts);
	hrtimer_cancel(&ts->timer);
 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
 fail:
	if (tma340_wq)
		destroy_workqueue(tma340_wq);
	return err;
}

static int tma340_remove(struct i2c_client *client)
{
	struct tma340 *ts = i2c_get_clientdata(client);
	struct tma340_platform_data *pdata;

	if (tma340_wq)
		destroy_workqueue(tma340_wq);

	pdata = client->dev.platform_data;
	pdata->exit_platform_hw();

	free_irq(MSM_GPIO_TO_INT(S1_GPIO_TOUCHPAD_nINT), ts);
	hrtimer_cancel(&ts->timer);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	input_unregister_device(ts->input);
	kfree(ts);

	return 0;
}

static struct i2c_device_id tma340_idtable[] = {
	{ "tma340", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tma340_idtable);

static struct i2c_driver tma340_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tma340"
	},
	.id_table	= tma340_idtable,
	.probe		= tma340_probe,
	.remove		= tma340_remove,
};

static int __init tma340_init(void)
{
	return i2c_add_driver(&tma340_driver);
}

static void __exit tma340_exit(void)
{
	i2c_del_driver(&tma340_driver);
}

module_init(tma340_init);
module_exit(tma340_exit);

MODULE_AUTHOR("Jeongpil Mok <jpmoks@sk-w.com>");
MODULE_DESCRIPTION("Cypress TMA340 TouchScreen Driver");
MODULE_LICENSE("GPL");
