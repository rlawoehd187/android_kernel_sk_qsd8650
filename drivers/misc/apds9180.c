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
#include <mach/gpio.h>
#include <mach/board-s1.h>




#define	APDS_MAGIC	'n'
#define	APDS_IOCTL_MAX	6

typedef struct
{
	unsigned char	ucPulseCnt;
	unsigned char	ucPulseDuty;
	unsigned char	ucPulseFreq;
	unsigned char	ucLEDCurrent;
	unsigned char	ucPulseInterval;
} APDS_PARAM_T;

#define APDS_SET_PARAM	_IOW (APDS_MAGIC, 0, APDS_PARAM_T)


typedef struct
{
	unsigned char	ucStatus;
	unsigned short	wData;
} APDS_DATA_T;

#define APDS_GET_DATA	_IOR (APDS_MAGIC, 1, APDS_DATA_T)

#define APDS_SET_POWER	_IOW (APDS_MAGIC, 2, unsigned int)



#define   CMD			0x80	//Command  
#define   ADDR_SD		0x00	//Shutdown Register address 
#define   ADDR_PFR		0x01	//Pulse_Freq Register address 
#define   ADDR_ICR		0x02	//Interval delay & Control Register address 
#define   ADDR_THRESLOW		0x03	//Threshold Low Register address 
#define   ADDR_THRESHIGH	0x04	//Threshold High Register address 
#define   ADDR_ADCDATALOW	0x05	//ADC Data Output Low Register address 
#define   ADDR_ADCDATAHIGH	0x06	//ADC Data Output Low Register address 
#define   ADDR_INTP		0x07	//Interrupt Register address 
#define   ADDR_MANUID		0x08	//Manufacturer ID Register 



#define   Pulse_4p		0x00 
#define   Pulse_8p		0x01 
#define   Pulse_12p		0x02 
#define   Pulse_16p		0x03 
#define   Pulse_20p		0x04 
#define   Pulse_24p		0x05 
#define   Pulse_28p		0x06 
#define   Pulse_32p		0x07 
 
#define   DutyCycle_12_5	0x00 
#define   DutyCycle_25_0	0x01 
#define   DutyCycle_37_5	0x02 
#define   DutyCycle_50_0	0x03 
 
#define   Freq_25Khz		0x00 
#define   Freq_50Khz		0x01 
#define   Freq_100Khz		0x02 
#define   Freq_200Khz		0x03 
 
#define   StopMeasure		0x00 
#define   StartMeasure		0x01 
 
#define   LEDCurrent75mA	0x00 
#define   LEDCurrent100mA	0x01 
#define   LEDCurrent125mA	0x02 
#define   LEDCurrent150mA	0x03 
 
#define   Interval_5ms		0x00 
#define   Interval_20ms		0x01 
#define   Interval_50ms		0x02 
#define   Interval_125ms	0x03 
#define   Interval_250ms	0x04 
#define   Interval_500ms	0x05 
#define   Interval_1s		0x06 
#define   Interval_2s		0x07 
 
#define   IntpDisable		0x00 
#define   IntpEOCEn		0x01 
#define   IntpThresEn		0x02 

#define   SoftwareReset		0x10 
#define   IntpClrEOC		0x20 
#define   IntpClrThres		0x40 
#define   IntpClrBoth		0x60


/*********************************************************************** 
Global Variable 
**********************************************************************/ 
static unsigned int UpperThreshold, LowerThreshold;      //Threshold values 
static struct i2c_client *m_pAPDSClient;




/*********************************************************************** 
Function Protoypes 
**********************************************************************/ 
static void PXS_Initialize(void); 
static void PXS_DevicePower(unsigned char value); 
static void PXS_PulseFreq(unsigned char PulseCount, unsigned char DutyCycle, unsigned char PulseFreq); 
static void PXS_IntvDelay(unsigned char FMeasurement, unsigned char LEDCurrent, unsigned char IntervalDelay); 
static void PXS_ThresHold(unsigned int Threshold); 
static void PXS_IntpEna(unsigned char value); 
static void PXS_IntpClr(unsigned char intpclear); 
static void PXS_ADCRead(u16 *wData) ;
static unsigned char PXS_IntpStatusRead(void); 
static unsigned char PXS_IDReg_Read(void);




/*********************************************************************** 
DESC:    Initialize sensor  
RETURNS: Nothing 
***********************************************************************/ 
static void PXS_Initialize(void) 
{ 
	UpperThreshold = 0x3E8;
	LowerThreshold = 0x320;

	PXS_IntpClr(SoftwareReset);

	PXS_DevicePower(1);
	PXS_IntpClr(IntpClrBoth);
	PXS_PulseFreq(Pulse_20p, DutyCycle_25_0, Freq_100Khz); 
	PXS_IntvDelay(StartMeasure, LEDCurrent100mA, Interval_5ms); 
	PXS_ThresHold(UpperThreshold);
	PXS_IntpEna(IntpThresEn);          
	PXS_IntpClr(IntpClrBoth);
}

/*********************************************************************** 
DESC:    0 for shutdown, oscillator and analog block turn off 
    1 for Power ON, measurement triggered by "start measurement" bit of interval delay/control register 
RETURNS: Nothing 
***********************************************************************/ 
static void PXS_DevicePower(unsigned char value)
{ 
	unsigned char command; 

	command = CMD| ADDR_SD;        
	i2c_smbus_write_byte_data(m_pAPDSClient, command, value); 
} 
 
/*********************************************************************** 
DESC:    Set the period, duty cycle and number of pulses for burst pulses 
RETURNS: Nothing 
***********************************************************************/ 
static void PXS_PulseFreq(unsigned char PulseCount, unsigned char DutyCycle, unsigned char PulseFreq) 
{ 
	unsigned char command, value; 

	command = CMD|ADDR_PFR;
	value = (PulseCount<<5) | (DutyCycle<<2) | PulseFreq;  
	i2c_smbus_write_byte_data(m_pAPDSClient, command, value); 
} 

/*********************************************************************** 
DESC:    Set the delay time between burst pulses, LED Current Control and start ADC measurement 
RETURNS: Nothing 
***********************************************************************/ 
static void PXS_IntvDelay(unsigned char FMeasurement, unsigned char LEDCurrent, unsigned char IntervalDelay) 
{  
	unsigned char command, value; 

	command = CMD|ADDR_ICR;
	value = (FMeasurement<<5) | (LEDCurrent<<3) | IntervalDelay ; 
	i2c_smbus_write_byte_data(m_pAPDSClient, command, value); 
} 
 
/*********************************************************************** 
DESC:    Set the interrupt threshold 
RETURNS: Nothing 
***********************************************************************/ 
static void PXS_ThresHold(unsigned int Threshold) 
{ 
	unsigned char command; 
	unsigned char TLoByte, THiByte; 

	TLoByte = (unsigned char)(Threshold);
	THiByte = (unsigned char)(Threshold >> 8);

	command = CMD|ADDR_THRESLOW;
	i2c_smbus_write_byte_data(m_pAPDSClient, command, TLoByte);  

	command = CMD|(ADDR_THRESHIGH);
	i2c_smbus_write_byte_data(m_pAPDSClient, command, THiByte);  
} 


/*********************************************************************** 
DESC:    Set the interrupt enable 
RETURNS: Nothing 
***********************************************************************/ 
 static void PXS_IntpEna(unsigned char value) 
 {  
 	unsigned char command; 
 
 	command = CMD|ADDR_INTP;
 	i2c_smbus_write_byte_data(m_pAPDSClient, command, value); 
 } 


/*********************************************************************** 
DESC:    Read back Interrupt Register Status 
RETURNS: Interrupt status 
***********************************************************************/  
static unsigned char PXS_IntpStatusRead(void) 
 {  
 	unsigned char command, intpstatus; 
 
 	command = CMD|ADDR_INTP;
 	intpstatus  = i2c_smbus_read_byte_data(m_pAPDSClient, command); 
 
 	return (intpstatus); 
 } 

 
/*********************************************************************** 
DESC:    Clear the pending thresholds & EOC interrupt or Reset  
RETURNS: Nothing 
************************************************************************/ 
static void PXS_IntpClr(unsigned char intpclear) 
{  
	i2c_smbus_write_byte(m_pAPDSClient, intpclear); 
}

/*********************************************************************** 
DESC:    Read ADC Data Output Value 
RETURNS: ADC Low Byte and ADC High Byte value 
***********************************************************************/ 
static void PXS_ADCRead(u16 *wData) 
{  
	unsigned char command; 

	command = CMD|(ADDR_ADCDATAHIGH);
	*wData  = i2c_smbus_read_byte_data(m_pAPDSClient, command) << 8;

	command = CMD|ADDR_ADCDATALOW;
	*wData |= i2c_smbus_read_byte_data(m_pAPDSClient, command); 
} 

/*********************************************************************** 
DESC:    Read Manufacturing ID Register Value 
RETURNS: Manufacturing ID Register Value 
***********************************************************************/ 
static unsigned char PXS_IDReg_Read(void) 
{ 
	unsigned char command, DeviceID; 

	command = CMD| ADDR_MANUID;        
	DeviceID = i2c_smbus_read_byte_data(m_pAPDSClient, command); 

	return(DeviceID); 
} 

#if 0
static void apds_work_func(struct work_struct *work)
{
	u_char	int_sts;
	u16	wData;
	
	int_sts = PXS_IntpStatusRead();
	PXS_ADCRead(&wData);

	if(int_sts & 0x20)
	{ 
		PXS_ThresHold(LowerThreshold);         //Set lower threshold values 
	} 
	else if(int_sts & 0x40)
	{
		PXS_ThresHold(UpperThreshold);         //Set upper threshold values 
	} 
	PXS_IntpClr(IntpClrBoth);              //Clear interrupt output 		
}

DECLARE_WORK(apds_work, apds_work_func);

static irqreturn_t apds_irq(int irq, void *dev_id)
{
 	if (irq == MSM_GPIO_TO_INT(S1_GPIO_PS_nINT))
 	{
		schedule_work(&apds_work);
		//printfk("APDS9180 : apds_irq occur \r\n");		
 	}
 	
 	return IRQ_HANDLED;
}
#endif


static int apds_open(struct inode *inode, struct file *filp)
{
	int	rc = 0;

    return (rc);
}

#define EOC_MASK	0x10

static ssize_t apds_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{	
	u_char	int_sts;
	u16	wData;
	u32	dwVal;

	int_sts = PXS_IntpStatusRead();
	PXS_ADCRead(&wData);
	PXS_IntpClr(IntpClrBoth);

	dwVal = (int_sts << 16) | wData;
	if (put_user(dwVal, (u32 __user *)buf))
	{
		return -EFAULT;
	}

	return sizeof(u32);
}

static ssize_t apds_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	u32	dwVal;
	u8	ucDelay, ucCurrent;

	if (get_user(dwVal, (u32 __user *)buf))
	{
		return  -EFAULT;
	}

	ucCurrent = (dwVal >> 16) & 0xff;
	ucDelay   = dwVal & 0xff;

	PXS_IntvDelay(StartMeasure, ucCurrent, ucDelay);
	return sizeof(u32);
}

static int apds_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;

	switch (cmd)
	{
		case APDS_SET_PARAM:
		{
			APDS_PARAM_T	sParam;
				
			if (copy_from_user(&sParam, (APDS_PARAM_T*)arg, sizeof(sParam)))
			{
				rc = -EFAULT;
				break;
			}
			PXS_PulseFreq(sParam.ucPulseCnt, sParam.ucPulseDuty, sParam.ucPulseFreq);
			PXS_IntvDelay(StartMeasure, sParam.ucLEDCurrent, sParam.ucPulseInterval); 
		}
		break;

		case APDS_GET_DATA:
		{
			APDS_DATA_T	sData;

			sData.ucStatus = PXS_IntpStatusRead();
			PXS_ADCRead(&sData.wData);
			PXS_IntpClr(IntpClrBoth);
			if (copy_to_user((APDS_DATA_T*)arg, &sData, sizeof(sData)))
			{
				rc = -EFAULT;
			}
		}
		break;

		case APDS_SET_POWER:
		{
			u32	dwVal;

			if (get_user(dwVal, (u32 __user *)arg))
			{
				return  -EFAULT;
			}
			PXS_DevicePower((unsigned char) dwVal);
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

static int apds_release(struct inode *inode, struct file *filp)
{
	int	rc = 0;

	return rc;
}

static const struct file_operations apds_fops = {
	.owner   = THIS_MODULE,
	.open    = apds_open,
	.read    = apds_read,
	.write    = apds_write,
	.ioctl   = apds_ioctl,
	.release = apds_release,
};

static struct miscdevice apds_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "apds",
	.fops = &apds_fops,
};


static int apds_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int 	rc;
	u_char 	ucID;
	u32     gpio_cfg = GPIO_CFG(S1_GPIO_PS_nINT, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);


	rc = gpio_tlmm_config(gpio_cfg, GPIO_ENABLE);
	if (rc)
		goto exit;

	m_pAPDSClient = client;
	ucID = PXS_IDReg_Read();
	if (ucID != 0x21)
	{
		rc = EIO;
		goto exit;
	}

	PXS_Initialize();
/*	
 	rc = request_irq(MSM_GPIO_TO_INT(S1_GPIO_PS_nINT),
 			 &apds_irq,
 			 IRQF_TRIGGER_FALLING,
 			 "apds_irq",
 			 NULL);
 	if (rc)
 		goto exit;
*/
	rc = misc_register(&apds_misc);
	if (unlikely(rc)) {
		printk(KERN_ERR "apds: failed to register misc device!\n");
		goto exit;
	}
	
	return (0);

exit:
	return rc;
}

static int apds_detect(struct i2c_client *client, int kind,
			 struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (!(adapter->class & I2C_CLASS_HWMON) && client->addr != 0x56)
		return -ENODEV;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)
	 && !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return -ENODEV;

	strlcpy(info->type, "apds", I2C_NAME_SIZE);	

	return 0;
}


static int apds_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));

	misc_deregister(&apds_misc);

	return 0;
}


static const struct i2c_device_id apds_id[] = {
	{ "apds", 0 },
	{ }
};


static struct i2c_driver apds_driver = {
	.driver = {
		.name	= "proxi",
	},
	.probe		= apds_probe,
	.remove		= apds_remove,
	.detect		= apds_detect,
	.id_table	= apds_id,
	.class		= I2C_CLASS_HWMON,
};

static int __init apds_init(void)
{
	int ret;

	ret = i2c_add_driver(&apds_driver);
	printk(KERN_WARNING	"apds_init : %d\n", ret);	

	return ret;
}

static void __exit apds_exit(void)
{
	i2c_del_driver(&apds_driver);	
}


MODULE_AUTHOR("TJ Kim");
MODULE_DESCRIPTION("APDS9180 Driver");
MODULE_LICENSE("GPL");


module_init(apds_init);
module_exit(apds_exit);


