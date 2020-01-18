/*
 * drivers/input/misc/bma150_I2C.c - bma150 driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <asm/gpio.h>
#include <linux/delay.h>

#include "BMA150calib.h"


#define BMA150_I2C_NAME "bma150"

#define BMAIO_MAGIC             0xB

/* BMA150 register address */
#define CHIP_ID_REG             0x00
#define VERSION_REG             0x01
#define ACC_X_LSB_REG           0x02
#define ACC_X_MSB_REG           0x03
#define ACC_Y_LSB_REG           0x04
#define ACC_Y_MSB_REG           0x05
#define ACC_Z_LSB_REG           0x06
#define ACC_Z_MSB_REG           0x07
#define TEMPERATURE_REG         0x08
#define STATUS_REG              0x09
#define CONTROL_REG             0x0a
#define CONFIGURE_REG           0x0b
#define LG_THRESHOLD_REG        0x0c
#define LG_DURATION_REG         0x0d
#define HG_THRESHOLD_REG        0x0e
#define HG_DURATION_REG         0x0f
#define ANY_MOTION_THRS_REG     0x10
#define HYSTERESIS_REG          0x11
#define CUSTOMER1_REG           0x12
#define CUSTOMER2_REG           0x13
#define RANGE_BWIDTH_REG        0x14
#define CONFIGURE2_REG          0x15

#define OFFSET_GAIN_X_REG       0x16
#define OFFSET_GAIN_Y_REG       0x17
#define OFFSET_GAIN_Z_REG       0x18
#define OFFSET_GAIN_T_REG       0x19
#define OFFSET_X_REG            0x1a
#define OFFSET_Y_REG            0x1b
#define OFFSET_Z_REG            0x1c
#define OFFSET_T_REG            0x1d

#define OFFSET_GAIN_X_EE_REG       0x36
#define OFFSET_GAIN_Y_EE_REG       0x37
#define OFFSET_GAIN_Z_EE_REG       0x38
#define OFFSET_GAIN_T_EE_REG       0x39
#define OFFSET_X_EE_REG            0x3a
#define OFFSET_Y_EE_REG            0x3b
#define OFFSET_Z_EE_REG            0x3c
#define OFFSET_T_EE_REG            0x3d



/* IOCTLs*/
#define BMA_IOCTL_INIT                    _IO(BMAIO_MAGIC, 0x0)
#define BMA_IOCTL_WRITE                  _IOW(BMAIO_MAGIC, 0x1, char[5])
#define BMA_IOCTL_READ                  _IOWR(BMAIO_MAGIC, 0x2, char[5])
#define BMA_IOCTL_READ_ACCELERATION     _IOWR(BMAIO_MAGIC, 0x3, short[3])
#define BMA_IOCTL_SET_MODE               _IOW(BMAIO_MAGIC, 0x4, short)
#define BMA_IOCTL_GET_INT                _IOR(BMAIO_MAGIC, 0x5, short)
#define BMA_IOCTL_CALIBRATION     		 _IOR(BMAIO_MAGIC, 0x6, unsigned short[3])



/* range and bandwidth */
#define BMA_RANGE_2G               0
#define BMA_RANGE_4G               1
#define BMA_RANGE_8G               2

#define BMA_BW_25HZ                0
#define BMA_BW_50HZ                1
#define BMA_BW_100HZ               2
#define BMA_BW_190HZ               3
#define BMA_BW_375HZ               4
#define BMA_BW_750HZ               5
#define BMA_BW_1500HZ              6

/* mode settings */
#define BMA_MODE_NORMAL            0
#define BMA_MODE_SLEEP             1

#define ORIENTATION_X	(0)
#define ORIENTATION_Y	(0)
#define ORIENTATION_Z	(-1)

static struct i2c_client *this_client;
static int bma_open_count=0;

struct bma150_data {
	struct i2c_client client;
	int irq;
};

char bma150_i2c_write(unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	int dummy;
#ifndef BMA150_SMBUS
	unsigned char buffer[2];
#endif
	if( this_client == NULL )	/*	No global client pointer?	*/
		return -1;

	while(len--)
	{
#ifdef BMA150_SMBUS
		dummy = i2c_smbus_write_byte_data(this_client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(this_client, (char*)buffer, 2);
#endif
		reg_addr++;
		data++;
		if(dummy < 0)
			return -1;
	}
	return 0;
}

/*	i2c read routine for bma150	*/
char bma150_i2c_read(unsigned char reg_addr, unsigned char *data, unsigned char len) 
{
	int dummy;
	if( this_client == NULL )	/*	No global client pointer?	*/
		return -1;

	while(len--)
	{        
#ifdef BMA150_SMBUS
		dummy = i2c_smbus_read_byte_data(this_client, reg_addr);
		if(dummy < 0)
			return -1;
		*data = dummy & 0x000000ff;
#else
		dummy = i2c_master_send(this_client, (char*)&reg_addr, 1);
		if(dummy < 0)
			return -1;
		dummy = i2c_master_recv(this_client, (char*)data, 1);
		if(dummy < 0)
			return -1;
#endif
		reg_addr++;
		data++;
	}
	return 0;
}

void bma150_i2c_delay(unsigned int msec)
{
	mdelay(msec);
}

static int bma150_read_data(unsigned char *data, int length)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = data,
		},
		{
		 .addr = this_client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = data,
		 },
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) < 0)
	{
		printk(KERN_ERR "%s: failed to read i2c data\n",__func__);
		return -EIO;
	}
	
	return 0;
}

static int bma150_write_data(unsigned char *data, int length)
{
	struct i2c_msg msg[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = data,
		 },
	};

	if(i2c_transfer(this_client->adapter, msg, 1) < 0)
	{
		printk(KERN_ERR "%s: failed to write i2c data\n", __func__);
		return -EIO;
	}
	
	return 0;
}

static int bma150_initialize(void)
{
	unsigned char buffer[8];
	int ret;

	buffer[0] = RANGE_BWIDTH_REG;
	ret = bma150_read_data(buffer, 1);
	if (ret < 0)
		return -1;	

	buffer[3] = buffer[1] | 1<<3;
	buffer[2] = CONFIGURE2_REG;
	buffer[1] = (buffer[0] & 0xe0) | (BMA_RANGE_2G<<3) | BMA_BW_25HZ;
	buffer[0] = RANGE_BWIDTH_REG;	
	ret = bma150_write_data(buffer, 4);
	if (ret < 0)
		return -1;

	mdelay(40);

	return 0;
}

static int bma150_transfer(short *rbuf)
{
	unsigned char buffer[2];
	int ret;	
	
	buffer[0] = ACC_X_LSB_REG;
	buffer[1] = 0;	
	ret = bma150_read_data(buffer, 2);	
	if (ret < 0) return 0;	
	rbuf[0] = (buffer[1] << 2) | ((buffer[0] & 0xC0) >> 6);
	if (rbuf[0] & 0x200) rbuf[0] -= 1<<10;

	buffer[0] = ACC_Y_LSB_REG;
	buffer[1] = 0;
	ret = bma150_read_data(buffer, 2);	
	if (ret < 0) return 0;	
	rbuf[1] = (buffer[1] << 2) | ((buffer[0] & 0xC0) >> 6);
	if (rbuf[1] & 0x200) rbuf[1] -= 1<<10;

	buffer[0] = ACC_Z_LSB_REG;
	buffer[1] = 0;
	ret = bma150_read_data(buffer, 2);	
	if (ret < 0) return 0;	
	rbuf[2] = (buffer[1] << 2) | ((buffer[0] & 0xC0) >> 6);
	if (rbuf[2] & 0x200) rbuf[2] -= 1<<10;

	return 1;
}

static int bma150_soft_reset(void)
{
	unsigned char buffer[2];
	int ret;

	buffer[0] = CONTROL_REG;
	ret = bma150_read_data(buffer, 1);
	if (ret < 0)
		return -1;

	buffer[1] = ( buffer[0] & 0xfd ) | 0x2;
	buffer[0] = CONTROL_REG;
	ret = bma150_write_data(buffer, 2);
	mdelay(40);

	return ret;
}

static int bma150_set_mode(char mode)
{
	unsigned char buffer[2];
	int ret;

	buffer[0] = CONTROL_REG;
	ret = bma150_read_data(buffer, 1);
	if (ret < 0)
		return -1;

	buffer[1] = ( buffer[0] & 0xfe ) | mode;
	buffer[0] = CONTROL_REG;
	ret = bma150_write_data(buffer, 2);

	if (mode == BMA_MODE_NORMAL)
		mdelay(40);
	return ret;
}

static int bma150_get_intr(void)
{
	int ret = 0;
	ret = gpio_get_value(this_client->irq);
	return ret;
}

static int bma_open(struct inode *inode, struct file *file)
{
	int err;

	if (++bma_open_count > 1) return 0;	

	err = bma150_set_mode(BMA_MODE_NORMAL);
	if (err < 0)
		printk(KERN_ERR "%s :failed to convert mode(NORMAL)\n", __func__);

	return err;
}

static int bma_release(struct inode *inode, struct file *file)
{
	int err;

	if(--bma_open_count > 0) return 0;

	err = bma150_set_mode(BMA_MODE_SLEEP);
	if (err < 0)
		printk(KERN_ERR "%s :failed to convert mode(SLEEP)\n", __func__);

	return err; 
}

static int bma_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

	void __user *argp = (void __user *)arg;

	unsigned char rwbuf[8];
	int ret = -1;
	short buf[3], temp;

	switch (cmd) {
	case BMA_IOCTL_READ:
	case BMA_IOCTL_WRITE:
	case BMA_IOCTL_SET_MODE:
		if (copy_from_user(&rwbuf, argp, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_from_user(&buf, argp, sizeof(buf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_CALIBRATION:
	{		
		smb380acc_t calData;
		smb380acc_t orient;
		smb380_t bma150;
	
		bma150.bus_write = bma150_i2c_write;
		bma150.bus_read = bma150_i2c_read;
		bma150.delay_msec = bma150_i2c_delay;
		smb380_init(&bma150);
		
		orient.x = ORIENTATION_X;
		orient.y = ORIENTATION_Y;
		orient.z = ORIENTATION_Z;		
		ret = bma150_calibrate(orient, &calData);		
		if (ret == 0) printk(KERN_WARNING	"calibrate : x=%d, y=%d, z=%d \n",calData.x, calData.y, calData.z);
		else printk(KERN_ERR	"calibrate : FAIL !!! \n");
		
		if (copy_to_user(argp, &calData, sizeof(calData)))
			return -EFAULT;
		return ret;
	}
	default:
		break;
	}

	switch (cmd) {
	case BMA_IOCTL_INIT:
		ret = bma150_initialize();
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_READ:
		if (rwbuf[0] < 1)
			return -EINVAL;
		ret = bma150_read_data(&rwbuf[1], rwbuf[0]);
//		rwbuf[1] = 0x02;
//		ret = bma150_read_data(&rwbuf[1], 1);
//		ret = rwbuf[1];
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_WRITE:
		if (rwbuf[0] < 2)
			return -EINVAL;
		ret = bma150_write_data(&rwbuf[1], rwbuf[0]);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		ret = bma150_transfer(&buf[0]);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_SET_MODE:
		bma150_set_mode(rwbuf[0]);
		break;
	case BMA_IOCTL_GET_INT:
		temp = bma150_get_intr();
		break;
	default:
		return -ENOTTY;
	}

	switch (cmd) {
	case BMA_IOCTL_READ:
		if (copy_to_user(argp, &rwbuf, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_to_user(argp, &buf, sizeof(buf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_GET_INT:
		if (copy_to_user(argp, &temp, sizeof(temp)))
			return -EFAULT;
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations bma_fops = {
	.owner = THIS_MODULE,
	.open = bma_open,
	.release = bma_release,
	.ioctl = bma_ioctl,
};

static struct miscdevice bma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bma150",
	.fops = &bma_fops,
};

int bma150_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bma150_data *bma;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	bma = kzalloc(sizeof(struct bma150_data), GFP_KERNEL);
	if (!bma) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, bma);

	this_client = client;

	err = bma150_soft_reset();
	if (err < 0) {
		printk(KERN_ERR "bma150_probe: failed to reset(SOFT)\n");
		goto exit_init_failed;
	}

	err = bma150_initialize();
	if (err < 0) {
		printk(KERN_ERR "bma150_probe: failed to initialize\n");
		goto exit_init_failed;
	}

	err = bma150_set_mode(BMA_MODE_SLEEP);
	if (err < 0) {
		printk(KERN_ERR "bma150_probe: failed to convert mode(NORMAL)\n");
		goto exit_init_failed;
	}

	this_client->irq = client->irq;

	err = misc_register(&bma_device);
	if (err) {
		printk(KERN_ERR "bma150_probe: failed to register device\n");
		goto exit_misc_device_register_failed;
	}

	return 0;

exit_misc_device_register_failed:
exit_init_failed:
	kfree(bma);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int bma150_remove(struct i2c_client *client)
{
	struct bma150_data *bma = i2c_get_clientdata(client);
	kfree(bma);
	return 0;
}

static const struct i2c_device_id bma150_id[] = {
	{ "bma", 0 },
	{ }
};

static struct i2c_driver bma150_driver = {
	.probe = bma150_probe,
	.remove = bma150_remove,
	.id_table = bma150_id,
	.driver = {
		   .name = BMA150_I2C_NAME,
		   },
};

static int __init bma150_init(void)
{
	printk(KERN_INFO "bma150 init\n");
	return i2c_add_driver(&bma150_driver);
}

static void __exit bma150_exit(void)
{
	i2c_del_driver(&bma150_driver);
}

module_init(bma150_init);
module_exit(bma150_exit);

