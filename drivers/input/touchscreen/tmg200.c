/*
 * drivers/input/touchscreen/tmg200.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
//#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tmg200.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>

#define TOUCH_READ_TOTAL_BYTES   13
#define TOUCH_Y_ERROR_DICISION_COUNT 7//6
#define TOUCH_Y_TOTAL_SENSOR_COUNT 10
#define TS_POLL_DELAY	(10 * 1000)
#define	MAX_12BIT ((1 << 12) - 1)

static struct workqueue_struct *tmg200_wq;
char buffer[TOUCH_READ_TOTAL_BYTES];

struct tmg200 {
	struct input_dev	*input;
	char			phys[32];
	//struct hrtimer		timer;
	struct early_suspend    early_suspend;
	struct work_struct      work;
	struct i2c_client	*client;
	spinlock_t		lock;

	u16			model;

	int			irq;
};

static int tmg200_read_values(struct tmg200 *tsc)
{
	int retry;

	struct i2c_msg msgs[] = {
		{
		 .addr = tsc->client->addr,
		 .flags = I2C_M_RD,
		 .len = TOUCH_READ_TOTAL_BYTES,
		 .buf = buffer,
		 },
	};

	buffer[0] = 0;
	for (retry = 0; retry <= 100; retry++) {
		if (i2c_transfer(tsc->client->adapter, msgs, 1) > 0)
			break;
		else
			mdelay(10);
	}
	if (retry > 100) {
		printk(KERN_ERR "%s: retry over 100\n",__func__);
		return -EIO;
	}	else

	return 0;
}

#define MAX_X 240
#define MAX_Y 400
void tmg200_convert_xy(int* x, int* y)
{
	int tmp_x, tmp_y;
	tmp_x = *x;
	tmp_y = *y;
	*x = abs(tmp_x - 240);
	*y = abs(tmp_y - 400);
}

static void tmg200_ts_work_func(struct work_struct *work)
{
	uint16_t x1, y1, x2, y2;
	int x1_int, y1_int;
	uint16_t sensormaskY;
	uint16_t countY=0;
	int i;

	struct tmg200 *ts = container_of(work, struct tmg200, work);

	tmg200_read_values(ts);
	x1 = (buffer[3]<<8) | buffer[4];
	y1 = (buffer[5]<<8) | buffer[6];
	x2 = (buffer[7]<<8) | buffer[8];
	y2 = (buffer[9]<<8) | buffer[10];
	sensormaskY = (buffer[12]<<2) | ((buffer[11]&0xC0)>>6);
	for(i=0;i<TOUCH_Y_TOTAL_SENSOR_COUNT;i++)
		countY += (sensormaskY >> i) & 0x1;

	if(buffer[1] != 0)
	{
//		printk(KERN_ERR "tmg200 value Mode : %d Gesture : %d Direction : %d\n", buffer[0], buffer[1], buffer[2]);
#if 0
		if(countY<TOUCH_Y_ERROR_DICISION_COUNT)
		{
			printk(KERN_ERR "tmg200 value X1: %d Y1: %d \n", x1, y1);
			if( (buffer[2]&0xF0) == 0x20 )
				printk(KERN_ERR "tmg200 value X2: %d Y2: %d\n", x2, y2);
		}
#endif
		x1_int = (int)x1;
		y1_int = (int)y1;
		tmg200_convert_xy(&x1_int, &y1_int);
		x1 = (uint16_t)x1_int;
		y1 = (uint16_t)y1_int;
//		printk(KERN_ERR "tmg200 converted value X1 : %d  Y1 : %d\n",x1*2, y1*2);
		input_report_abs(ts->input, ABS_X, x1*2);
		input_report_abs(ts->input, ABS_Y, y1*2);
		input_report_key(ts->input, BTN_TOUCH, 1);
		input_report_abs(ts->input, ABS_PRESSURE, 1);
		input_sync(ts->input);
	}
	else if(buffer[1] == 0)
	{
		input_report_abs(ts->input, ABS_PRESSURE, 0);
		input_report_key(ts->input, BTN_TOUCH, 0);
		input_sync(ts->input);
	}
	enable_irq(ts->client->irq);
}

static irqreturn_t tmg200_irq(int irq, void *handle)
{
	struct tmg200 *ts = handle;

	disable_irq(ts->irq);

	queue_work(tmg200_wq, &ts->work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void tmg200_early_suspend(struct early_suspend *h)
{
	struct tmg200 *ts;
	ts = container_of(h, struct tmg200, early_suspend);
	disable_irq(ts->irq);
	cancel_work_sync(&ts->work);
}

void tmg200_late_resume(struct early_suspend *h)
{
	struct tmg200 *ts;
	ts = container_of(h, struct tmg200, early_suspend);
	enable_irq(ts->irq);
}
#endif

static int tmg200_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tmg200 *ts;
	struct tmg200_platform_data *pdata = pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err;

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	tmg200_wq = create_singlethread_workqueue("tmg200_wq");
	if (!tmg200_wq) {
		err = -ENOMEM;
		goto fail;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "tmg200_probe: need I2C_FUNC_I2C\n");
		err = -ENODEV;
	}

	ts = kzalloc(sizeof(struct tmg200), GFP_KERNEL);

	INIT_WORK(&ts->work, tmg200_ts_work_func);
	
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->input = input_dev;

	spin_lock_init(&ts->lock);

	err = pdata->init_platform_hw();
	if (err != 0)
		goto err_free_mem;
	

	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "TMG200 Touchscreen";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_SYN);
	input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(input_dev, ABS_X, 0, 480, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 800, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_12BIT, 0, 0);

	ts->irq = client->irq;

	err = request_irq(ts->irq, tmg200_irq, 0,
			client->dev.driver->name, ts);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = tmg200_early_suspend;
	ts->early_suspend.resume = tmg200_late_resume;
#endif
	register_early_suspend(&ts->early_suspend);

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	dev_info(&client->dev, "registered with irq (%d)\n", ts->irq);

	return 0;

 err_free_irq:
	free_irq(ts->irq, ts);
	//hrtimer_cancel(&ts->timer);
	unregister_early_suspend(&ts->early_suspend);
 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
 fail:
	if (tmg200_wq)
		destroy_workqueue(tmg200_wq);
	return err;
}

static int tmg200_remove(struct i2c_client *client)
{
	struct tmg200 *ts = i2c_get_clientdata(client);
	struct tmg200_platform_data *pdata;

	if (tmg200_wq)
		destroy_workqueue(tmg200_wq);

	pdata = client->dev.platform_data;
	pdata->exit_platform_hw();

	free_irq(ts->irq, ts);
	//hrtimer_cancel(&ts->timer);
	unregister_early_suspend(&ts->early_suspend);
	input_unregister_device(ts->input);
	kfree(ts);

	return 0;
}

static struct i2c_device_id tmg200_idtable[] = {
	{ "tmg200", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tmg200_idtable);

static struct i2c_driver tmg200_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tmg200"
	},
	.id_table	= tmg200_idtable,
	.probe		= tmg200_probe,
	.remove		= tmg200_remove,
};

static int __init tmg200_init(void)
{
	return i2c_add_driver(&tmg200_driver);
}

static void __exit tmg200_exit(void)
{
	i2c_del_driver(&tmg200_driver);
}

module_init(tmg200_init);
module_exit(tmg200_exit);

MODULE_AUTHOR("Jeongpil Mok <jpmoks@sk-w.com>");
MODULE_DESCRIPTION("Cypress TouchScreen Driver");
MODULE_LICENSE("GPL");
