/*  amosense magnetic field sensor chips driver
*
* Copyright (C) 2010 Amosense Co., Ltd
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
  
/*
*	Release Note
*  2010-04-07 [Version 1.1.0] : add AMS0303M Driver
*  2010-03-30 [Version 1.0.0] : Release AMS0303H Driver
*/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/ams0303.h>
#include <mach/gpio.h>
#include <mach/board-s1.h>


// Device input report
/*
#define		AMS_SENSORS_REPORT_MAG_X							ABS_HAT0X
#define		AMS_SENSORS_REPORT_MAG_Y							ABS_HAT0Y
#define		AMS_SENSORS_REPORT_MAG_Z							ABS_BRAKE

#define		AMS_SENSORS_REPORT_ACC_X							ABS_X
#define		AMS_SENSORS_REPORT_ACC_Y							ABS_Y
#define		AMS_SENSORS_REPORT_ACC_Z							ABS_Z

#define		AMS_SENSORS_REPORT_YAW								ABS_RX
#define		AMS_SENSORS_REPORT_PITCH								ABS_RY
#define		AMS_SENSORS_REPORT_ROLL								ABS_RZ
#define		AMS_SENSORS_REPORT_STATUS							ABS_RUDDER
*/
// Sensors active flag
/*
#define		AMS_SENSORS_ACTIVE_ACCELEROMETER			0x01
#define		AMS_SENSORS_ACTIVE_MAGNETIC_FIELD				0x02
#define		AMS_SENSORS_ACTIVE_ORIENTATION					0x04
*/

// DEVICE FILE NAME
#define		AMS_DEVICE_FILE_NAME		"ams0303"
//#define		AMS_DAEMON_FILE_NAME	"ams0303_d"
//#define		AMS_DEVICE_INPUT_NAME	"compassinput"

// DEVICE INFO
#define		AMS_DEVICE_NAME				"ams0303"
#define		AMS_DEVICE_ID				0x04

// AMS0303H REGISTERS
#define		AMS_REG_RESET				0x00
#define		AMS_REG_ID					0x01
#define		AMS_REG_REVISION			0x02
#define		AMS_REG_POWER				0x03

#define		AMS_REG_CONTROL_3			0x04
#define		AMS_REG_CONTROL_2			0x21
#define		AMS_REG_CONTROL				0x2E

#define		AMS_REG_DATA				0x06

#define		AMS_REG_DAC_X1H				0x22
#define		AMS_REG_DAC_X2H				0x24
#define		AMS_REG_DAC_Y1H				0x26
#define		AMS_REG_DAC_Y2H				0x28
#define		AMS_REG_DAC_Z1H				0x2A
#define		AMS_REG_DAC_Z2H				0x2C
#define		AMS_REG_TEST_ADC			0x40
#define		AMS_REG_TEST_KEY			0x50
#define		AMS_REG_TEST_MODE			0x51
#define		AMS_REG_OTP_DATA			0x53
#define		AMS_REG_OTP_CMD				0x55

// constant
//#define		DEVICE_SELECT_AMS0303H		0
//#define		DEVICE_SELECT_AMS0303M		1

#define		AMS0303H_GAIN				0x01
#define		AMS0303M_GAIN				0x07

#define		AMS_DEVICE_LIMIT_DAC		10
#define		AMS_DEVICE_DAC_MAX			1000
#define		AMS_DEVICE_DAC_MIN			20
#define		AMS_DEVICE_MAX_VALUE		1023


// struct
/*
struct ams0303_mag_data
{
	struct input_dev *pInputDev;
	struct work_struct work;
};
*/

// ams0303h variable
static struct	i2c_client	*ams_mag_client; 

static int g_nStatus = 0;
static int g_nSumDacXYZ[6] = {512,512,512,512,512,512};
static int g_nBeforeSumDacXYZ[6];
static int g_nDacXYZ[6] = {512,512,512,512,512,512};
static int g_nDDacXYZ[6] = {0,0,0,0,0,0};
static unsigned int g_nOTP = 0;
static int g_nMagData[3];
static int g_nMagActiveFlag = 0;
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_ES01)
static int g_modelType=AMS_MODEL_TYPE_0303H;
static int g_nGain=AMS0303H_GAIN;
#else
static int g_modelType=AMS_MODEL_TYPE_0303M;
static int g_nGain=AMS0303M_GAIN;
#endif


// sensors 
//static int g_nSensorsData[10];
//static int g_nSensorsActiveMask = 0;

static int AMSInit(void);

static const unsigned short normal_i2c[] = { 0x50>>1, I2C_CLIENT_END };
I2C_CLIENT_INSMOD_1(ams0303);

//////////////////////////////////////////////////////////////////////////
// GPIO Config
static int AMSConfig_gpio(int config)
{
	u32 gpio_cfg_int, gpio_cfg_rst;
	int rc;

	if (config) 
	{	
		gpio_cfg_int = GPIO_CFG(S1_GPIO_COMPASS_nINT, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);	
		gpio_cfg_rst = GPIO_CFG(S1_GPIO_COMPASS_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
	}
	else 
	{
		gpio_cfg_int = GPIO_CFG(S1_GPIO_COMPASS_nINT, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);	
		gpio_cfg_rst = GPIO_CFG(S1_GPIO_COMPASS_nRST, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA);
	}
	
	rc = gpio_tlmm_config(gpio_cfg_rst, GPIO_ENABLE);
	if (rc) goto exit;

	rc = gpio_tlmm_config(gpio_cfg_int, GPIO_ENABLE);
	if (rc) goto exit;

	if (config)
	{
		gpio_set_value(S1_GPIO_COMPASS_nRST, 0);
		mdelay(10);
		gpio_set_value(S1_GPIO_COMPASS_nRST, 1);
		mdelay(10);
	}
	else
	{
		gpio_set_value(S1_GPIO_COMPASS_nRST, 0);
		mdelay(10);
	}
 
	/*
	if (config) {	// for wake state
		gpio_tlmm_config(GPIO_CFG(1, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(2, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(80, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(91, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	} else {		// for sleep state
		gpio_tlmm_config(GPIO_CFG(1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(2, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(80, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(91, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	}
	*/	

	return 0;
exit:
	return rc; 
}

/*
static int AMSSet_vreg(int onoff)
{
	
	struct vreg *vreg_compass_30;
	struct vreg *vreg_compass_27;
	int err;

	vreg_compass_30 = vreg_get(0, "gp4");
	vreg_compass_27 = vreg_get(0, "gp3");

	if (onoff)
	{
		vreg_enable(vreg_compass_30);
		vreg_enable(vreg_compass_27);

		err = vreg_set_level(vreg_compass_30, 3000);
		if (err != 0) {
			printk("vreg_compass_30 failed.\n");
			return -1;
		}

		err = vreg_set_level(vreg_compass_27, 2700);
		if (err != 0) {
			printk("vreg_compass_27 failed.\n");
			return -1;
		}
	}
	else
	{
		vreg_disable(vreg_compass_30);
		vreg_disable(vreg_compass_27);
	}

	return 0;

}
*/

//////////////////////////////////////////////////////////////////////////
// ams sensors function
static void AMSMagEnable(void)
{	
	//g_nSensorsActiveMask |= AMS_SENSORS_ACTIVE_MAGNETIC_FIELD;	

	//AMSSet_vreg(1);

	msleep(1);
	AMSInit();	
}

static void AMSMagDisable(void)
{
	//g_nSensorsActiveMask &=~AMS_SENSORS_ACTIVE_MAGNETIC_FIELD;	

	//AMSSet_vreg(0);
}
/*
static void AMSAccEnable(void)
{
	g_nSensorsActiveMask |= AMS_SENSORS_ACTIVE_ACCELEROMETER;
}

static void AMSAccDisable(void)
{
	g_nSensorsActiveMask &=~AMS_SENSORS_ACTIVE_ACCELEROMETER;
}

static void AMSCompassEnable(void)
{
	g_nSensorsActiveMask |= AMS_SENSORS_ACTIVE_ORIENTATION;	
}

static void AMSCompassDisable(void)
{
	g_nSensorsActiveMask &=~AMS_SENSORS_ACTIVE_ORIENTATION;	
}
*/

/*
static int ReportData(void)
{
	struct ams0303_mag_data *pData = i2c_get_clientdata(ams_mag_client);

	if( g_nSensorsActiveMask &  AMS_SENSORS_ACTIVE_MAGNETIC_FIELD )
	{
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_MAG_X, g_nSensorsData[0]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_MAG_Y, g_nSensorsData[1]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_MAG_Z, g_nSensorsData[2]);
	}

	if( g_nSensorsActiveMask &  AMS_SENSORS_ACTIVE_ACCELEROMETER )
	{
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_ACC_X, g_nSensorsData[3]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_ACC_Y, g_nSensorsData[4]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_ACC_Z, g_nSensorsData[5]);
	}

	if( g_nSensorsActiveMask &  AMS_SENSORS_ACTIVE_ORIENTATION )
	{
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_YAW, g_nSensorsData[6]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_PITCH, g_nSensorsData[7]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_ROLL, g_nSensorsData[8]);
		input_report_abs(pData->pInputDev, AMS_SENSORS_REPORT_STATUS, g_nSensorsData[9]);
	}

	input_sync(pData->pInputDev);

	return 0;
}
*/

//////////////////////////////////////////////////////////////////////////
// ams0303h function
static int ReadDevice(unsigned char *readBuffer, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr = ams_mag_client->addr,
				.flags = 0,
				.len = 1,
				.buf = readBuffer,
		},
		{
			.addr = ams_mag_client->addr,
				.flags = I2C_M_RD,
				.len = length,
				.buf = readBuffer,
			},
	};

	if (i2c_transfer(ams_mag_client->adapter, msgs, 2) < 0) 
	{		
		printk(KERN_ERR "Read error\n");
		return -EIO;
	} else
	{
		return 0;
	}
}

static int WriteDevice(unsigned char *writeBuffer, int length)
{
	struct i2c_msg msg[] = 
	{
		{
			.addr = ams_mag_client->addr,
				.flags = 0,
				.len = length,
				.buf = writeBuffer,
		},
	};

	if (i2c_transfer(ams_mag_client->adapter, msg, 1) < 0) 
	{
		printk(KERN_ERR "Write error\n");
		return -EIO;
	} else
		return 0;
}

static int Reset(void)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_RESET;
	writeBuffer[1] = 0x01;
	return WriteDevice(writeBuffer, 2);
}

static int PowerUp(void)
{
	unsigned char writeBuffer[2];
	
	writeBuffer[0] = AMS_REG_POWER;
	writeBuffer[1] = 0x9F;
	WriteDevice(writeBuffer, 2);

	// Continuous Mode start
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0xBE;
	return WriteDevice(writeBuffer, 2);
}

static int PowerDown(void)
{
	unsigned char writeBuffer[2];

	// Continuous Mode stop
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_POWER;
	writeBuffer[1] = 0x00;
	return WriteDevice(writeBuffer, 2);
}

static int SetStabilizedTime(int nData)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_CONTROL_3;
	writeBuffer[1] = (unsigned char)(nData&0xFF);
	return WriteDevice(writeBuffer, 2);
}

static int EnableContinuousMode(void)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0xBE;
	return WriteDevice(writeBuffer, 2);
}

static int DisableContinuousMode(void)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0x00;
	return WriteDevice(writeBuffer, 2);
}

static int GetDeviceID(void)
{
	unsigned char readBuffer[2];
	readBuffer[0] = AMS_REG_ID;
	ReadDevice(readBuffer, 1);
	return (int)(readBuffer[0]);
}

static int GetRevisionNumber(void)
{
	unsigned char readBuffer[2];
	readBuffer[0] = AMS_REG_REVISION;
	ReadDevice(readBuffer, 1);
	return (int)(readBuffer[0]);	
}

static int CheckID(void)
{	
	if( GetDeviceID() == AMS_DEVICE_ID )
		return 0;

	return -1;
}

static int GetStatus(void)
{
	return g_nStatus;
}

static int OpenOTP(void)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_RESET;
	writeBuffer[1] = 0x15;
	WriteDevice(writeBuffer, 2);

	msleep(1);

	writeBuffer[0] = AMS_REG_TEST_KEY;
	writeBuffer[1] = 0xD4;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_TEST_MODE;
	writeBuffer[1] = 0x04;
	WriteDevice(writeBuffer, 2);
	
	return 0;
}

static int CloseOTP(void)
{
	unsigned char writeBuffer[2];
	writeBuffer[0] = AMS_REG_TEST_MODE;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_TEST_KEY;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);
	
	writeBuffer[0] = AMS_REG_RESET;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);
	
	return 0;
}

static int ReadOTP(void)
{
	short nTemp;
	unsigned char deviceBuffer[3];
	
	deviceBuffer[0] = AMS_REG_POWER;
	deviceBuffer[1] = 0xFF;
	WriteDevice(deviceBuffer, 2);

	msleep(1);
	if( OpenOTP() < 0 )
		return -1;

	msleep(1);
	deviceBuffer[0] = AMS_REG_OTP_CMD;
	deviceBuffer[1] = 0x81;
	WriteDevice(deviceBuffer, 2);
	
	msleep(1);
	
	deviceBuffer[0] = AMS_REG_OTP_DATA;
	ReadDevice(deviceBuffer, 2);
	nTemp = (short)( (deviceBuffer[0]<<8) | (deviceBuffer[1]&0xFF) );
	
	CloseOTP();
	
	return (int)nTemp;
}

static int LoadDACOffset(void)
{
	unsigned char writeBuffer[13];

	g_nSumDacXYZ[0] = g_nDacXYZ[0] + g_nDDacXYZ[0];
	g_nSumDacXYZ[1] = g_nDacXYZ[1] + g_nDDacXYZ[1];
	g_nSumDacXYZ[2] = g_nDacXYZ[2] + g_nDDacXYZ[2];
	g_nSumDacXYZ[3] = g_nDacXYZ[3] + g_nDDacXYZ[3];
	g_nSumDacXYZ[4] = g_nDacXYZ[4] + g_nDDacXYZ[4];
	g_nSumDacXYZ[5] = g_nDacXYZ[5] + g_nDDacXYZ[5];

	writeBuffer[0] = AMS_REG_CONTROL_2;
	writeBuffer[1] = (unsigned char)(  g_nGain&0xFF );
	WriteDevice(writeBuffer, 2);		

	if( g_nBeforeSumDacXYZ[2] != g_nSumDacXYZ[2] )
	{
		writeBuffer[0] = AMS_REG_DAC_X1H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[2]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[2]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[2] = g_nSumDacXYZ[2];
	}

	if( g_nBeforeSumDacXYZ[3] != g_nSumDacXYZ[3] )
	{
		writeBuffer[0] = AMS_REG_DAC_X2H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[3]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[3]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[3] = g_nSumDacXYZ[3];
	}

	if( g_nBeforeSumDacXYZ[0] != g_nSumDacXYZ[0] )
	{
		writeBuffer[0] = AMS_REG_DAC_Y1H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[0]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[0]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[0] = g_nSumDacXYZ[0];
	}

	if( g_nBeforeSumDacXYZ[1] != g_nSumDacXYZ[1] )
	{
		writeBuffer[0] = AMS_REG_DAC_Y2H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[1]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[1]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[1] = g_nSumDacXYZ[1];
	}

	if( g_nBeforeSumDacXYZ[4] != g_nSumDacXYZ[4] )
	{
		writeBuffer[0] = AMS_REG_DAC_Z1H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[4]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[4]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[4] = g_nSumDacXYZ[4];
	}

	if( g_nBeforeSumDacXYZ[5] != g_nSumDacXYZ[5] )
	{
		writeBuffer[0] = AMS_REG_DAC_Z2H;
		writeBuffer[1] = (unsigned char)( (g_nSumDacXYZ[5]>>8)&0xFF );
		writeBuffer[2] = (unsigned char)( (g_nSumDacXYZ[5]&0xFF) );
		WriteDevice(writeBuffer, 3);

		g_nBeforeSumDacXYZ[5] = g_nSumDacXYZ[5];
	}
	
	return 0;
}

static int GetData(void)
{
	int nMx00 = 0;
	int nMx90 = 0;
	int nMy00 = 0;
	int nMy90 = 0;
	int nMz00 = 0;
	int nMz90 = 0;
	//int i=0;
	int	nRet = 0;
	unsigned char readBuffer[13];

	if( DisableContinuousMode() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to set continuous mode stop\n", AMS_DEVICE_NAME);		
		return 0;
	}
	
	readBuffer[0] = AMS_REG_DATA;
	ReadDevice(readBuffer, 12);
	
	nMx00 = (int)( (short)((readBuffer[4]<<8) |  (readBuffer[5]&0xFF)) );
	nMy00 = (int)( (short)((readBuffer[0]<<8) |  (readBuffer[1]&0xFF)) );
	nMz00 = (int)( (short)((readBuffer[8]<<8) |  (readBuffer[9]&0xFF)) );

	nMx90 = (int)( (short)((readBuffer[6]<<8) |  (readBuffer[7]&0xFF)) );
	nMy90 = (int)( (short)((readBuffer[2]<<8) |  (readBuffer[3]&0xFF)) );
	nMz90 = (int)( (short)((readBuffer[10]<<8) |  (readBuffer[11]&0xFF)) );

	// auto dac calibration
	if( (nMx00 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[0]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[0]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMx00 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[0]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[0]-=AMS_DEVICE_LIMIT_DAC;

	if( (nMy00 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[2]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[2]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMy00 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[2]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[2]-=AMS_DEVICE_LIMIT_DAC;

	if( (nMz00 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[4]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[4]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMz00 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[4]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[4]-=AMS_DEVICE_LIMIT_DAC;

	//
	if( (nMx90 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[1]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[1]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMx90 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[1]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[1]-=AMS_DEVICE_LIMIT_DAC;

	if( (nMy90 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[3]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[3]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMy90 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[3]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[3]-=AMS_DEVICE_LIMIT_DAC;

	if( (nMz90 > AMS_DEVICE_DAC_MAX) && (g_nSumDacXYZ[5]<(AMS_DEVICE_MAX_VALUE-AMS_DEVICE_LIMIT_DAC)) )
		g_nDDacXYZ[5]+=AMS_DEVICE_LIMIT_DAC;

	if( (nMz90 < AMS_DEVICE_DAC_MIN) && (g_nSumDacXYZ[5]>AMS_DEVICE_LIMIT_DAC) )
		g_nDDacXYZ[5]-=AMS_DEVICE_LIMIT_DAC;

	if( LoadDACOffset() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to load dac offset\n", AMS_DEVICE_NAME);
		nRet = -1;
	}
	
	if( SetStabilizedTime(0x00) < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to set stabilized time\n", AMS_DEVICE_NAME);
		nRet = -1;
	}
	
	if( EnableContinuousMode() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to set continuout mode start\n", AMS_DEVICE_NAME);
		nRet = -1;
	}
		
	if( nRet == 0 )
	{		
		g_nMagData[0] = (nMx90-nMx00) + (g_nDDacXYZ[1]-g_nDDacXYZ[0]) * 15;
		g_nMagData[1] = (nMy00-nMy90) + (g_nDDacXYZ[2]-g_nDDacXYZ[3]) * 15;
		g_nMagData[2] = (nMz90-nMz00) + (g_nDDacXYZ[5]-g_nDDacXYZ[4]) * 15;			
	}
	PowerUp();

	return 0;
}
/*
static int InputDevicePoll(void)
{
	if( GetData() < 0 )
		return -1;

	return 0;
}
*/
static int AMSInit(void)
{	
	// reset
	if( Reset() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed Reset\n", AMS_DEVICE_NAME);
		return -1;
	}

	// Check ID
	if( CheckID() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to ID\n", AMS_DEVICE_NAME);
		return -1;
	}

	// otp
	g_nOTP = ReadOTP();	
	
	// power up
	if( PowerUp() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to power up\n", AMS_DEVICE_NAME);
		return -1;
	}

	if( LoadDACOffset() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to Load DAC\n", AMS_DEVICE_NAME);
		return -1;
	}

	if( SetStabilizedTime(0x02) < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to set stabilized time\n", AMS_DEVICE_NAME);
		return -1;
	}

	if( EnableContinuousMode() < 0 )
	{
		printk(KERN_ERR "%s Driver: Failed to enable continuout mode\n", AMS_DEVICE_NAME);
		return -1;
	}
	
	return 0;
}

static int DeviceEnable(void)
{
	if( g_nMagActiveFlag == 0 )
	{	
		PowerUp();
		g_nMagActiveFlag = 1;				
	}
	return 0;
}

static int DeviceDisable(void)
{
	if( g_nMagActiveFlag == 1 )
	{	
		g_nMagActiveFlag = 0;
		PowerDown();				
	}
	return 0;
}

static int FactoryTest(void)
{
	int nRet = 0;
	unsigned char writeBuffer[2];
	unsigned char readBuffer[13];
	int AMS0303_TestBuf[6];
	int i=0;
	int AMS0303_Ok_cnt = 0;

	/** ID Check */
	if( CheckID() < 0 )
		nRet = -1;

	/** Voltage Status	*/
	// Enter Test Mode
	writeBuffer[0] = AMS_REG_RESET;
	writeBuffer[1] = 0x15;
	WriteDevice(writeBuffer, 2);

	msleep(1);

	writeBuffer[0] = AMS_REG_TEST_KEY;
	writeBuffer[1] = 0xD4;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_TEST_MODE;
	writeBuffer[1] = 0x04;
	WriteDevice(writeBuffer, 2);

	// Select Positive Drive
	writeBuffer[0] = AMS_REG_TEST_ADC;
	writeBuffer[1] = 0xA0;
	WriteDevice(writeBuffer, 2);

	// Power All ON
	writeBuffer[0] = AMS_REG_POWER;
	writeBuffer[1] = 0xFF;
	WriteDevice(writeBuffer, 2);

	msleep(1);

	// Data stabilized time setting - Init Filter
	writeBuffer[0] = AMS_REG_CONTROL_3;
	writeBuffer[1] = 0x02;
	WriteDevice(writeBuffer, 2);

	// Continuous Mode Start
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0xBE;
	WriteDevice(writeBuffer, 2);

	msleep(100);

	// Continuous mode stop
	writeBuffer[0] = AMS_REG_CONTROL;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	// read data
	readBuffer[0] = AMS_REG_DATA;
	ReadDevice(readBuffer, 12);

	AMS0303_TestBuf[0] = (int)( (short)((readBuffer[0]<<8) |  (readBuffer[1]&0xFF)) );
	AMS0303_TestBuf[1] = (int)( (short)((readBuffer[2]<<8) |  (readBuffer[3]&0xFF)) );
	AMS0303_TestBuf[2] = (int)( (short)((readBuffer[4]<<8) |  (readBuffer[5]&0xFF)) );

	AMS0303_TestBuf[3] = (int)( (short)((readBuffer[6]<<8) |  (readBuffer[7]&0xFF)) );
	AMS0303_TestBuf[4] = (int)( (short)((readBuffer[8]<<8) |  (readBuffer[9]&0xFF)) );
	AMS0303_TestBuf[5] = (int)( (short)((readBuffer[10]<<8) |  (readBuffer[11]&0xFF)) );

	for(i=0;i<6;i++)
	{
		if(AMS0303_TestBuf[i]>=400 && AMS0303_TestBuf[i]<=600)
			AMS0303_Ok_cnt++;
	}

	if(AMS0303_Ok_cnt==6)
	{
		nRet=0;
	}
	else 
	{
		nRet=-2;
	}

	// exit
	writeBuffer[0] = AMS_REG_RESET;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	msleep(1);

	writeBuffer[0] = AMS_REG_TEST_KEY;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_TEST_MODE;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_TEST_ADC;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	writeBuffer[0] = AMS_REG_POWER;
	writeBuffer[1] = 0x00;
	WriteDevice(writeBuffer, 2);

	return nRet;
}

// ams_fops
static int AMSOpen(struct inode *inode, struct file *file)
{		
	DeviceEnable();
	AMSMagEnable();	
	return 0;
}

static int AMSRelease(struct inode *inode, struct file *file)
{	
	AMSMagDisable();
	DeviceDisable();
	
	return 0;	
}

static int AMSIOCTL(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int nRet=0;

	switch( cmd )
	{		
	case AMS_IOCTL_MAG_RESET :
		if( Reset() < 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_POWER_UP :
		if( PowerUp() < 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_POWER_DOWN :
		if( PowerDown() < 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_READ_ID :
		nRet = GetDeviceID();
		if( nRet < 0 )
			nRet = -EFAULT;

		if( copy_to_user(argp, &nRet, sizeof(nRet)) != 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_READ_REVISION :
		nRet = GetRevisionNumber();
		if( nRet < 0 )
			nRet = -EFAULT;

		if( copy_to_user(argp, &nRet, sizeof(nRet)) != 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_READ_STATUS :
		nRet = GetStatus();
		if( nRet < 0 )
			nRet = -EFAULT;

		if( copy_to_user(argp, &nRet, sizeof(nRet)) != 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_READ_OTP :	
		nRet = 0;
		if( copy_to_user(argp, &g_nOTP, sizeof(g_nOTP)) != 0 )
		{
			nRet = -EFAULT;
		}
		break;

	case AMS_IOCTL_MAG_READ_DATA :
		nRet = GetData();
		if( nRet < 0 )
			nRet = -EFAULT;

		if( copy_to_user(argp, &g_nMagData, sizeof(g_nMagData)) != 0 )
			nRet = -EFAULT;
		break;

	case AMS_IOCTL_MAG_ENABLE :
		nRet = DeviceEnable();
		break;

	case AMS_IOCTL_MAG_DISABLE :
		nRet = DeviceDisable();
		break;
/*
	case AMS_DAEMON_IOCTL_MAG_ENABLE :
		DeviceEnable();
		AMSMagEnable();		
		break;

	case AMS_DAEMON_IOCTL_MAG_DISABLE :
		AMSMagDisable();
		DeviceDisable();
		break;

	case AMS_DAEMON_IOCTL_ACC_ENABLE :
		AMSAccEnable();
		break;

	case AMS_DAEMON_IOCTL_ACC_DISABLE :
		AMSAccDisable();
		break;

	case AMS_DAEMON_IOCTL_COMPASS_ENABLE :
		AMSCompassEnable();
		DeviceEnable();
		break;

	case AMS_DAEMON_IOCTL_COMPASS_DISABLE :
		AMSCompassDisable();
		DeviceDisable();
		break;
*/
	case AMS_IOCTL_FACTORY_TEST :
		nRet = FactoryTest();
		if( copy_to_user(argp, &nRet, sizeof(nRet)) != 0 )
		{
			nRet = -EFAULT;
		}
		break;

	case AMS_IOCTL_MAG_MODEL_TYPE :		
		if( copy_to_user(argp, &g_modelType, sizeof(g_modelType)) != 0 )
		{
			nRet = -EFAULT;
		}
		break;
	}

	return nRet;
}



// ams_fops
/*
static int AMSDOpen(struct inode *inode, struct file *file)
{		
	return 0;
}

static int AMSDRelease(struct inode *inode, struct file *file)
{	
	return 0;	
}
*/
/*
static void AMSSelectDevice(int nDevice)
{
	switch (nDevice)
	{
	case DEVICE_SELECT_AMS0303H :		
		g_nGain = AMS0303H_GAIN;
		break;

	case DEVICE_SELECT_AMS0303M :		
		g_nGain = AMS0303M_GAIN;
		break;

	default :
		break;
	}
}
*/
/*
static int AMSDIOCTL(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int nRet=0;

	switch( cmd )
	{
	case AMS_DAEMON_IOCTL_MAG_READ_ID :
		nRet = GetDeviceID();
		if( nRet < 0 )
			nRet = -EFAULT;

		if( copy_to_user(argp, &nRet, sizeof(nRet)) != 0 )
			nRet = -EFAULT;
		break;
	case AMS_DAEMON_IOCTL_READ_DATA :		
		nRet = 0;
		GetData();
		if( copy_to_user(argp, &g_nMagData, sizeof(g_nMagData)) != 0 )
		{
			nRet = -EFAULT;
		}		
		break;

	case AMS_DAEMON_IOCTL_READ_OTP :
		nRet = 0;
		AMSInit();		
		if( copy_to_user(argp, &g_nOTP, sizeof(g_nOTP)) != 0 )
		{
			nRet = -EFAULT;
		}
		break;

	case AMS_DAEMON_IOCTL_READ_FLAG :
		nRet = 0;
		if( copy_to_user(argp, &g_nSensorsActiveMask, sizeof(g_nSensorsActiveMask)) != 0 )
		{
			nRet = -EFAULT;
		}
		break;

	case AMS_DAEMON_IOCTL_WRITE_DATA :
		nRet = 0;
		if( copy_from_user(&g_nSensorsData, argp, sizeof(g_nSensorsData)) != 0 )
		{
			nRet = -EFAULT;
		}
		ReportData();
		break;

	case AMS_DAEMON_IOCTL_DRIVER_INIT :
		AMSInit();
		break;

	case AMS_DAEMON_IOCTL_SELECT_DEVICE :	
		nRet = 0;
		if( copy_from_user(&nRet, argp, sizeof(nRet)) != 0 )
		{
			nRet = -EFAULT;
		}
		AMSSelectDevice(nRet);
		break;
	}	

	return nRet;
}
*/
//////////////////////////////////////////////////////////////////////////
// daemon interface function
/*  
static struct	file_operations	ams0303_daemon_fops =
{
	.owner = THIS_MODULE,
	.open = AMSDOpen,
	.release = AMSDRelease,
	.ioctl = AMSDIOCTL,
};

static struct miscdevice ams0303_daemon_misc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = AMS_DAEMON_FILE_NAME,
	.fops = &ams0303_daemon_fops,
};
*/
//////////////////////////////////////////////////////////////////////////
// device interface function
static struct	file_operations	ams0303_mag_fops =
{
	.owner = THIS_MODULE,
	.open = AMSOpen,
	.release = AMSRelease,
	.ioctl = AMSIOCTL,
};

static struct miscdevice ams0303_mag_misc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = AMS_DEVICE_FILE_NAME,
	.fops = &ams0303_mag_fops,
};


#ifdef CONFIG_PM
static int ams0303_pm_suspend(struct device *dev)
{
	g_nMagActiveFlag = 0;
	PowerDown();

	AMSConfig_gpio(0);

	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static int ams0303_pm_resume(struct device *dev)
{
	AMSConfig_gpio(1);
	g_nMagActiveFlag = 1;

	printk(KERN_ERR "%s\n", __func__);

	return 0;
}

static struct dev_pm_ops ams0303_pm_ops = {
	.suspend = ams0303_pm_suspend,
	.resume  = ams0303_pm_resume,
};
#endif


static int ams0303_probe(struct i2c_client *pClient, const struct i2c_device_id *pID)
{
	int nError = 0;
//	struct ams0303_mag_data	*pAmsData;

	printk(KERN_INFO "%s Magnetic Field Sensor Driver, (C)2010 Amosense Co.,Ltd\n", AMS_DEVICE_NAME);

	//if( !i2c_check_functionality(pClient->adapter, I2C_FUNC_I2C) )
	if (!i2c_check_functionality(pClient->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)
	 && !i2c_check_functionality(pClient->adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK)) 
	{
		printk(KERN_ERR "%s : Failed to check functionaliry\n", AMS_DEVICE_NAME);
		nError = -ENODEV;

		goto exit_check_functionality_failed;
	}
/*
	pAmsData = kzalloc( sizeof(struct ams0303_mag_data), GFP_KERNEL );
	if( !pAmsData )
	{
		printk(KERN_ERR "%s : Failed to allocate data\n", AMS_DEVICE_NAME);
		nError = -ENOMEM;

		goto exit_alloc_data_failed;
	}
*/
//	i2c_set_clientdata(pClient, pAmsData);
	
	ams_mag_client = pClient;

	AMSConfig_gpio(1);
/*
	// input
	pAmsData->pInputDev = input_allocate_device();
	if( !pAmsData->pInputDev )
	{
		printk(KERN_ERR "%s : Failed to allocate input device\n", AMS_DEVICE_NAME);
		nError = -ENOMEM;

		goto exit_input_dev_alloc_failed;
	}
*/
/*
	set_bit(EV_ABS, pAmsData->pInputDev->evbit);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_MAG_X, -2048, 2032, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_MAG_Y, -2048, 2032, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_MAG_Z, -2048, 2032, 0, 0);	

	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_ACC_X, -2048, 2032, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_ACC_Y, -2048, 2032, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_ACC_Z, -2048, 2032, 0, 0);	

	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_YAW, 0, 360, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_PITCH, -90, 90, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_ROLL, -90, 90, 0, 0);
	input_set_abs_params(pAmsData->pInputDev, AMS_SENSORS_REPORT_STATUS, 0, 3, 0, 0);
*/
	// register input
/*
	pAmsData->pInputDev->name = AMS_DEVICE_INPUT_NAME;
	nError = input_register_device(pAmsData->pInputDev);
	if( nError )
	{
		printk(KERN_INFO "ams sensors - Failed to input retister device\n");
		goto exit_input_register_device_failed;
	}
*/	
	// register daemon file
/* 
	nError = misc_register(&ams0303_daemon_misc);
	if( nError )
	{
		printk(KERN_ERR "%s : Failed to register misc(daemon)\n", AMS_DEVICE_NAME);
		goto exit_misc_register_failed;
	}
*/
	// register control file
	nError = misc_register(&ams0303_mag_misc);
	if( nError )
	{
		printk(KERN_ERR "%s : Failed to register misc\n", AMS_DEVICE_NAME);
		goto exit_misc_register_failed;
	}

	return 0;

exit_misc_register_failed:
//exit_input_register_device_failed:
//exit_input_dev_alloc_failed:
	//kfree(pAmsData);
//exit_alloc_data_failed:
exit_check_functionality_failed:

	return nError;
}

static int ams0303_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{	
	struct i2c_adapter *adapter = client->adapter;

	if (!(adapter->class & I2C_CLASS_HWMON) && client->addr != 0x28)
		return -ENODEV;
	
	strlcpy(info->type, AMS_DEVICE_NAME, I2C_NAME_SIZE);
	return 0;
}

static int ams0303_remove(struct i2c_client *pClient)
{
/*
	struct ams0303_mag_data	*pAmsData;
	
	pAmsData = i2c_get_clientdata(pClient);
	
	kfree(pAmsData);
*/
	misc_deregister(&ams0303_mag_misc); 

	return 0;
}

static const struct i2c_device_id		ams0303_id[] = 
{
	{AMS_DEVICE_NAME, 0},
	{ }
};

static struct i2c_driver	ams0303_driver = 
{
	.class = I2C_CLASS_HWMON,
	.probe = ams0303_probe,
	.remove = ams0303_remove,
	.id_table = ams0303_id,
	.driver = {
		.owner = THIS_MODULE,
		.name  = AMS_DEVICE_NAME,
#ifdef CONFIG_PM
		.pm    = &ams0303_pm_ops,
#endif
	},
	.detect = ams0303_detect,
	.address_data = &addr_data,
};

static int __init ams0303_init(void)
{
	return i2c_add_driver(&ams0303_driver);
}

static void __exit ams0303_exit(void)
{
	i2c_del_driver(&ams0303_driver);
}

module_init(ams0303_init);
module_exit(ams0303_exit);

MODULE_AUTHOR("Gyu-Bok Sim <hana@amosense.co.kr>");
MODULE_DESCRIPTION("Amosense magnetic field sensor driver and sensors manager");
MODULE_LICENSE("GPL");


