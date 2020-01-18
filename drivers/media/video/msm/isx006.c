/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board-s1.h>
#include "isx006.h"
#include "camsensor_tune.h"
#include <linux/wakelock.h>


typedef enum
{
    ISX006_I2C_WRITE_MODE_NORMAL,
    ISX006_I2C_WRITE_MODE_SEQUENTIAL,
    ISX006_I2C_WRITE_MODE_SEQUENTIAL_END
} isx006_i2c_write_mode_t;

static isx006_i2c_write_mode_t isx006_i2c_write_mode = ISX006_I2C_WRITE_MODE_NORMAL;
static int isx006_i2c_sequential_write_first_time = 1;
static int isx006_i2c_cnt = 0;
#define I2C_BUF_MAX 1500
unsigned char i2c_buf[I2C_BUF_MAX];


struct isx006_work {
	struct work_struct work;
};

static struct  isx006_work *isx006_sensorw;
static struct  i2c_client *isx006_client;

struct isx006_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};

static struct isx006_ctrl *isx006_ctrl;

static struct wake_lock rearcam_wake_lock;

static int g_enable_i2c_klog_main = 0;

static DECLARE_WAIT_QUEUE_HEAD(isx006_wait_queue);
DECLARE_MUTEX(isx006_sem);

static int isx006_prev_wb = -1;
static int isx006_prev_brightness = -1;
static int isx006_prev_exposure_mode = -1;
static int isx006_prev_iso_mode = -1;
static int isx006_prev_scene_mode = 0;
static int isx006_prev_ffps = -1;
static int isx006_prev_sensor_mode = -1;
static int isx006_af_driver_check_first_time = 1;
static int isx006_af_driver_ready = 0;
static int isx006_prev_af_mode = -1;
static int isx006_prev_reflect = 0;
static int isx006_prev_jpeg_quality = -1;
static int isx006_prev_zoom = -1;
static int isx006_prev_zoom_width = -1;
static int isx006_prev_zoom_height = -1;
static int isx006_prev_vout_size = 600;
static int isx006_second_init_tbl_written = 0;
static int isx006_delayed_scene_mode_setting = 0;
static int isx006_snapshot_done = 0;
static int isx006_suspend_disable = 0;


/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct isx006_reg isx006_regs;


/*=============================================================*/

static int isx006_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
	u32 cfg = 0;
	struct vreg *vreg_cam;

	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_STANDBY, 1);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_nRST, 1);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_EN, 0);

	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_nRST, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_EN, 1);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_EN, 1);
	vreg_cam = vreg_get(NULL, "gp1"); vreg_set_level(vreg_cam, 2600); vreg_enable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp6"); vreg_set_level(vreg_cam, 2800); vreg_enable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp2"); vreg_set_level(vreg_cam, 2800); vreg_enable(vreg_cam);
	cfg = GPIO_CFG(S1_GPIO_CAM_CLK_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_CAM_CLK_EN, 1);		
	
	mdelay(10);

	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_nRST, 1);

    mdelay(10);
	
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 1);

	mdelay(20);
		
	return rc;
}


static int32_t isx006_i2c_txdata(unsigned short saddr, unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(isx006_client->adapter, msg, 1) < 0) {
		return -EIO;
	}

	return 0;
}


#if defined(FEATURE_SKTS_CAM_SENSOR_SEQUENTIAL_WRITE_MODE)
static int32_t isx006_i2c_write(unsigned short saddr, unsigned short waddr, unsigned short wdata, unsigned short width)
{
	int32_t rc = -EIO;
	int32_t data_len = 0;
	int8_t  retry_cnt = 0;

	if (waddr == 0xFFFF)
	{
		mdelay(wdata);
		return 0;
	}

	if (waddr == 0 && wdata == 0)
	{
		return 0;
	}
	else if (waddr == 0 && wdata == 1)
	{
        isx006_i2c_write_mode = ISX006_I2C_WRITE_MODE_SEQUENTIAL;
        isx006_i2c_sequential_write_first_time = 1;
        return 0;
	}
	else if (waddr == 0 && wdata == 2)
	{
        isx006_i2c_write_mode = ISX006_I2C_WRITE_MODE_SEQUENTIAL_END;
	}
	
	switch (isx006_i2c_write_mode)
	{
	case ISX006_I2C_WRITE_MODE_NORMAL:
    	memset(i2c_buf, 0, 4);
    	switch (width)
    	{
    	case 16:
    		i2c_buf[0] = (waddr & 0xFF00)>>8;
    		i2c_buf[1] = (waddr & 0x00FF);
    		i2c_buf[2] = (wdata & 0x00FF);
    		i2c_buf[3] = (wdata & 0xFF00)>>8;
    		data_len = 4;
    		break;

    	case 8:
    		i2c_buf[0] = (waddr & 0xFF00)>>8;
    		i2c_buf[1] = (waddr & 0x00FF);
    		i2c_buf[2] = (wdata & 0x00FF);
    		data_len = 3;
    		break;
    		
    	default:
    		break;
    	}
    	break;
    	
    case ISX006_I2C_WRITE_MODE_SEQUENTIAL:
        if (isx006_i2c_sequential_write_first_time)
        {
            isx006_i2c_cnt = 0;
            memset(i2c_buf, 0, sizeof(i2c_buf));
        	switch (width)
        	{
        	case 16:
        		i2c_buf[isx006_i2c_cnt++] = (waddr & 0xFF00)>>8;
        		i2c_buf[isx006_i2c_cnt++] = (waddr & 0x00FF);
        		i2c_buf[isx006_i2c_cnt++] = (wdata & 0x00FF);
        		i2c_buf[isx006_i2c_cnt++] = (wdata & 0xFF00)>>8;
        		break;

        	case 8:
        		i2c_buf[isx006_i2c_cnt++] = (waddr & 0xFF00)>>8;
        		i2c_buf[isx006_i2c_cnt++] = (waddr & 0x00FF);
        		i2c_buf[isx006_i2c_cnt++] = (wdata & 0x00FF);
        		break;
        		
        	default:
        		break;
        	}
            isx006_i2c_sequential_write_first_time = 0;
        }
        else
        {
            if (isx006_i2c_cnt >= I2C_BUF_MAX)
            {
                return -EINVAL;
            }
            
            switch (width)
            {
            case 16:
        		i2c_buf[isx006_i2c_cnt++] = (wdata & 0x00FF);
        		i2c_buf[isx006_i2c_cnt++] = (wdata & 0xFF00)>>8;
                break;

            case 8:
                i2c_buf[isx006_i2c_cnt++] = (wdata & 0x00FF);
                break;

            default:
                break;                
            }
        }
        return 0;
        
    case ISX006_I2C_WRITE_MODE_SEQUENTIAL_END:
        isx006_i2c_write_mode = ISX006_I2C_WRITE_MODE_NORMAL;
        data_len = isx006_i2c_cnt;
        break;

    default :
        return -EINVAL;
	}
	
	
	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = isx006_i2c_txdata(saddr, i2c_buf, data_len);
		if (rc < 0)
		{
			mdelay(100);
		}
		else
		{
			break;
		}
	}

	return rc;
}
#else
static int32_t isx006_i2c_write(unsigned short saddr, unsigned short waddr, unsigned short wdata, unsigned short width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	int8_t data_len = 0;
	int8_t retry_cnt = 0;

	if (waddr == 0xFFFF)
	{
		mdelay(wdata);
		return 0;
	}

	if (waddr == 0 && wdata == 0)
	{
		return 0;
	}

	memset(buf, 0, sizeof(buf));
	
	switch (width)
	{
	case 16:
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0x00FF);
		buf[3] = (wdata & 0xFF00)>>8;
		data_len = 4;
		break;

	case 8:
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0x00FF);
		data_len = 3;
		break;
		
	default:
		break;
	}

	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = isx006_i2c_txdata(saddr, buf, data_len);
		if (rc < 0)
		{
			mdelay(100);
		}
		else
		{
			break;
		}
	}

	return rc;
}
#endif


static int32_t isx006_i2c_write_table(struct isx006_i2c_reg_conf const *reg_conf_tbl, int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = isx006_i2c_write(isx006_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}


static int isx006_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(isx006_client->adapter, msgs, 2) < 0) {
		return -EIO;
	}

	return 0;
}


static int32_t isx006_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, unsigned short width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	int8_t retry_cnt = 0;

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = isx006_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
		{
			mdelay(100);
		}
		else
		{
			break;
		}
	}

	if (retry_cnt < 3)
	{
		switch (width)
		{
		case 16:
			*rdata = buf[1] << 8 | buf[0];
			break;
		case 8:
			*rdata = buf[0];
			break;
		default:
			break;
		}
	}

	return rc;
}


static int32_t isx006_i2c_read_long(unsigned short   saddr,
	unsigned short raddr, unsigned long *rdata)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	int8_t retry_cnt = 0;

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = isx006_i2c_rxdata(saddr, buf, 4);
		if (rc < 0)
		{
			mdelay(100);
		}
		else
		{
			break;
		}
	}

	if (retry_cnt < 3)
	{
		*rdata = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
	}

	return rc;
}


static long isx006_reg_init(void)
{
	long rc;
#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
    struct isx006_i2c_reg_conf *tune_init_tbl = NULL;
    uint16_t tune_init_tbl_size;
#endif
    unsigned long rdata = 0;

    wake_lock(&rearcam_wake_lock);
    isx006_suspend_disable = 1;

	g_enable_i2c_klog_main = 0;

    isx006_i2c_read_long(isx006_client->addr, 0x0368, &rdata);

    if (((rdata >> 26) & 0x0000000F) == 0x1)
    {
        rc = isx006_i2c_write_table(&isx006_regs.dpc_ap003_tbl[0], isx006_regs.dpc_ap003_tbl_size);
    }
    else if (((rdata >> 26) & 0x0000000F) == 0x4)
    {
        rc = isx006_i2c_write_table(&isx006_regs.dpc_ap001_tbl[0], isx006_regs.dpc_ap001_tbl_size);
    }
    else
    {
        return -EIO;
    }

#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
	rc = camsensor_set_file_read(ISX006_INIT_REG_FILE, (uint32_t *)&tune_init_tbl, &tune_init_tbl_size);
	if (rc < 0)
	{
		rc = isx006_i2c_write_table(&isx006_regs.init_tbl[0], isx006_regs.init_tbl_size);
	}
	else
	{
		rc = isx006_i2c_write_table((struct isx006_i2c_reg_conf *)tune_init_tbl, tune_init_tbl_size);
	}
#else
	rc = isx006_i2c_write_table(&isx006_regs.init_tbl[0], isx006_regs.init_tbl_size);
#endif

	if (rc < 0)
		return rc;

	return 0;
}


#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
static long isx006_reg_init_2(void)
{
	long rc;
#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
    struct isx006_i2c_reg_conf *tune_init_tbl_2 = NULL;
    uint16_t tune_init_tbl_2_size;
    struct isx006_i2c_reg_conf *tune_scene_tbl = NULL;
    uint16_t tune_scene_tbl_size;
#endif

	if (isx006_second_init_tbl_written == 1)
		return 0;

	g_enable_i2c_klog_main = 0;

#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
	rc = camsensor_set_file_read(ISX006_INIT_REG_2_FILE, (uint32_t *)&tune_init_tbl_2, &tune_init_tbl_2_size);
	if (rc < 0)
	{
		rc = isx006_i2c_write_table(&isx006_regs.init_tbl_2[0], isx006_regs.init_tbl_2_size);
	}
	else
	{
		rc = isx006_i2c_write_table((struct isx006_i2c_reg_conf *)tune_init_tbl_2, tune_init_tbl_2_size);
	}
#else
	rc = isx006_i2c_write_table(&isx006_regs.init_tbl_2[0], isx006_regs.init_tbl_2_size);
#endif

	if (rc < 0)
		return rc;

    if (isx006_delayed_scene_mode_setting == 1)
    {
    	g_enable_i2c_klog_main = 1;

#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
    	rc = camsensor_set_file_read(ISX006_SCENE_REG_FILE, (uint32_t *)&tune_scene_tbl, &tune_scene_tbl_size);
    	if (rc < 0)
    	{
    		rc = isx006_i2c_write_table(isx006_regs.scene_tbl + isx006_regs.scene_tbl_size * isx006_prev_scene_mode, isx006_regs.scene_tbl_size);
    	}
    	else
    	{
    		rc = isx006_i2c_write_table(tune_scene_tbl + ISX006_SCENE_TBL_SIZE * isx006_prev_scene_mode, ISX006_SCENE_TBL_SIZE);
    	}
#else
    	rc = isx006_i2c_write_table(isx006_regs.scene_tbl + isx006_regs.scene_tbl_size * isx006_prev_scene_mode, isx006_regs.scene_tbl_size);
#endif

    	if (rc < 0)
    		return rc;
    }

    isx006_second_init_tbl_written = 1;

	return 0;
}
#endif


static long isx006_set_effect(int mode, int effect)
{
	return 0;
}


static long isx006_set_wb(int wb)
{
	long rc;
	uint16_t wb_sel;

	if (isx006_prev_wb == wb)
		return 0;

    if (isx006_prev_scene_mode > 0)
    {
        return 0;
    }
	
	switch(wb)
	{
	case 1:
		wb_sel = 0;
		break;
	case 3:
		wb_sel = 4;
		break;
	case 4:
		wb_sel = 3;
		break;
	case 5:
		wb_sel = 2;
		break;
	case 6:
		wb_sel = 1;
		break;
	default:
		return -EINVAL;		
	}

	rc = isx006_i2c_write_table(isx006_regs.wb_tbl + isx006_regs.wb_tbl_size * wb_sel, isx006_regs.wb_tbl_size);
	
	if (rc < 0)
		return rc;

	isx006_prev_wb = wb;

	return 0;
}


static long isx006_set_brightness(int brightness)
{
	long rc;

	if (isx006_prev_brightness == brightness)
		return 0;

    if (isx006_prev_scene_mode > 0)
    {
        return 0;
    }

	if (brightness > 6 || brightness < 0)
	{
		return -EINVAL;
	}

	rc = isx006_i2c_write_table(isx006_regs.brightness_tbl + isx006_regs.brightness_tbl_size * brightness, isx006_regs.brightness_tbl_size);

	if (rc < 0)
		return rc;

	isx006_prev_brightness = brightness;

	return 0;
}


static long isx006_set_exposure_mode(int exposure_mode)
{
	long rc;

	if (isx006_prev_exposure_mode == exposure_mode)
		return 0;

    if (isx006_prev_scene_mode > 0)
    {
        return 0;
    }

	if (exposure_mode < 0 || exposure_mode > 2)
	{
		return -EINVAL;
	}
	
	rc = isx006_i2c_write_table(isx006_regs.exposure_tbl + isx006_regs.exposure_tbl_size * exposure_mode, isx006_regs.exposure_tbl_size);
	
	if (rc < 0)
		return rc;

	isx006_prev_exposure_mode = exposure_mode;

	return 0;
}


static long isx006_set_iso_mode(int iso_mode)
{
	long rc;
	uint16_t iso_sel;

	if (isx006_prev_iso_mode == iso_mode)
		return 0;

    if (isx006_prev_scene_mode > 0)
    {
        return 0;
    }

	switch(iso_mode)
	{
	case 0:
		iso_sel = 0;
		break;
	case 2:
		iso_sel = 1;
		break;
	case 3:
		iso_sel = 2;
		break;
	case 4:
		iso_sel = 3;
		break;
	default:
		return -EINVAL;		
	}

	rc = isx006_i2c_write_table(isx006_regs.iso_tbl + isx006_regs.iso_tbl_size * iso_sel, isx006_regs.iso_tbl_size);

	if (rc < 0)
		return rc;

	isx006_prev_iso_mode = iso_mode;

	return 0;
}


static long isx006_set_scene_mode(int scene_mode)
{
	long rc;
#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
    struct isx006_i2c_reg_conf *tune_scene_tbl = NULL;
    uint16_t tune_scene_tbl_size;
#endif

	if (isx006_prev_scene_mode == scene_mode)
		return 0;

	if (scene_mode < 0 || scene_mode > 10)
	{
		return -EINVAL;
	}

#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
    if (isx006_second_init_tbl_written == 0)
    {
        isx006_delayed_scene_mode_setting = 1;
       	isx006_prev_scene_mode = scene_mode;
       	return 0;
    }
#endif

#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
	rc = camsensor_set_file_read(ISX006_SCENE_REG_FILE, (uint32_t *)&tune_scene_tbl, &tune_scene_tbl_size);
	if (rc < 0)
	{
		rc = isx006_i2c_write_table(isx006_regs.scene_tbl + isx006_regs.scene_tbl_size * scene_mode, isx006_regs.scene_tbl_size);
	}
	else
	{
		rc = isx006_i2c_write_table(tune_scene_tbl + ISX006_SCENE_TBL_SIZE * scene_mode, ISX006_SCENE_TBL_SIZE);
	}
#else
	rc = isx006_i2c_write_table(isx006_regs.scene_tbl + isx006_regs.scene_tbl_size * scene_mode, isx006_regs.scene_tbl_size);
#endif

	if (rc < 0)
		return rc;

	isx006_prev_scene_mode = scene_mode;

	return 0;
}


static long isx006_set_frame_rate(int ffps)
{
	long rc;
	uint16_t ffps_sel;

	if (isx006_prev_ffps == ffps)
		return 0;

	switch (ffps)
	{
	case  8 :
		ffps_sel = 0;
        break;
	case  9 :
		ffps_sel = 1;
        break;
	case 10 :
		ffps_sel = 2;
        break;
	case 11 :
		ffps_sel = 3;
        break;
	case 12 :
		ffps_sel = 4;
		break;
	case 15 :
		ffps_sel = 5;
		break;
	default :
		isx006_prev_ffps = ffps;
		return 0;
	}

    rc = isx006_i2c_write_table(isx006_regs.fixed_fps_tbl + isx006_regs.fixed_fps_tbl_size * ffps_sel, isx006_regs.fixed_fps_tbl_size);

	if (rc < 0)
	    return rc;

	isx006_prev_ffps = ffps;

	return 0;
}


static long isx006_af_driver_ready_check(void)
{
	unsigned short rdata1 = 0, rdata2 = 0;

	if (isx006_af_driver_check_first_time == 1)
	{
		isx006_i2c_read(isx006_client->addr, 0x000a, &rdata1, 8);
		isx006_i2c_read(isx006_client->addr, 0x6D76, &rdata2, 8);

		if (rdata1 == 0x02 && rdata2 == 0x03)
		{
			isx006_af_driver_ready = 1;
		}
		else
		{
			isx006_af_driver_ready = 0;
		}
		isx006_af_driver_check_first_time = 0;
	}

	return isx006_af_driver_ready;
}


static long isx006_set_af_mode(int af_mode)
{
    g_enable_i2c_klog_main = 0;

	if (isx006_prev_af_mode == af_mode)
	    return 0;

	switch (af_mode)
	{
	case 0:
	case 1:
	case 2:
		isx006_i2c_write(isx006_client->addr, 0x4C4C, 0x03E8, 16); // AF_OPD4_HDELAY : 3A
		isx006_i2c_write(isx006_client->addr, 0x4C4E, 0x02A4, 16); // AF_OPD4_VDELAY : 
		isx006_i2c_write(isx006_client->addr, 0x4C7C, 0x06, 8);    // AF_OPD1A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7D, 0x05, 8);    // AF_OPD1B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7E, 0x02, 8);    // AF_OPD2A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7F, 0x01, 8);    // AF_OPD2B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C80, 0x08, 8);    // AF_OPD3A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C81, 0x07, 8);    // AF_OPD3B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C82, 0x02, 8);    // AF_OPD4A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C83, 0x01, 8);    // AF_OPD4B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C84, 0x04, 8);    // AF_OPD5A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C85, 0x03, 8);    // AF_OPD5B_WEIGHT
		break;
		
	case 3:
		isx006_i2c_write(isx006_client->addr, 0x4C7C, 0x00, 8); //AF_OPD1A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7D, 0x00, 8); //AF_OPD1B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7E, 0x00, 8); //AF_OPD2A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C7F, 0x00, 8); //AF_OPD2B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C80, 0x08, 8); //AF_OPD3A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C81, 0x00, 8); //AF_OPD3B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C82, 0x00, 8); //AF_OPD4A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C83, 0x00, 8); //AF_OPD4B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C84, 0x00, 8); //AF_OPD5A_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x4C85, 0x00, 8); //AF_OPD5B_WEIGHT
		isx006_i2c_write(isx006_client->addr, 0x484E, 0x08, 8); //AF_OPD_WEIGHT_TH
		break;
	}

	switch (af_mode)
	{
	case 0:
	case 2:
	case 3:
		isx006_i2c_write(isx006_client->addr, 0x00FC, 0x1F, 8);
		isx006_i2c_write(isx006_client->addr, 0x01D3, 0x04, 8); //AF_SN1
		isx006_i2c_write(isx006_client->addr, 0x01D4, 0x04, 8); //AF_SN2
		isx006_i2c_write(isx006_client->addr, 0x01D5, 0x04, 8); //AF_SN3
		isx006_i2c_write(isx006_client->addr, 0x01D6, 0x04, 8); //AF_SN4
		isx006_i2c_write(isx006_client->addr, 0x01D7, 0x04, 8); //AF_SN5
		isx006_i2c_write(isx006_client->addr, 0x01D8, 0x04, 8); //AF_SN6
		isx006_i2c_write(isx006_client->addr, 0x01D9, 0x04, 8); //AF_SN7
		isx006_i2c_write(isx006_client->addr, 0x01DA, 0x04, 8); //AF_SN8
		isx006_i2c_write(isx006_client->addr, 0x01DB, 0x04, 8); //AF_SN9
		isx006_i2c_write(isx006_client->addr, 0x01DC, 0x04, 8); //AF_SN10
		isx006_i2c_write(isx006_client->addr, 0x01DD, 0x04, 8); //AF_SN11
		isx006_i2c_write(isx006_client->addr, 0x01DE, 0x04, 8); //AF_SN12
		isx006_i2c_write(isx006_client->addr, 0x002E, 0x02, 8);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x4852, 0x0082, 16);
		isx006_i2c_write(isx006_client->addr, 0x4850, 0x01, 8);
		break;

	case 1:
		isx006_i2c_write(isx006_client->addr, 0x00FC, 0x1F, 8);
		isx006_i2c_write(isx006_client->addr, 0x01D3, 0x02, 8); //AF_SN1
		isx006_i2c_write(isx006_client->addr, 0x01D4, 0x02, 8); //AF_SN2
		isx006_i2c_write(isx006_client->addr, 0x01D5, 0x02, 8); //AF_SN3
		isx006_i2c_write(isx006_client->addr, 0x01D6, 0x02, 8); //AF_SN4
		isx006_i2c_write(isx006_client->addr, 0x01D7, 0x02, 8); //AF_SN5
		isx006_i2c_write(isx006_client->addr, 0x01D8, 0x02, 8); //AF_SN6
		isx006_i2c_write(isx006_client->addr, 0x01D9, 0x02, 8); //AF_SN7
		isx006_i2c_write(isx006_client->addr, 0x01DA, 0x02, 8); //AF_SN8
		isx006_i2c_write(isx006_client->addr, 0x01DB, 0x02, 8); //AF_SN9
		isx006_i2c_write(isx006_client->addr, 0x01DC, 0x02, 8); //AF_SN10
		isx006_i2c_write(isx006_client->addr, 0x01DD, 0x02, 8); //AF_SN11
		isx006_i2c_write(isx006_client->addr, 0x01DE, 0x02, 8); //AF_SN12
		isx006_i2c_write(isx006_client->addr, 0x002E, 0x02, 8);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x4852, 0x0242, 16);
		isx006_i2c_write(isx006_client->addr, 0x4850, 0x01, 8);
		break;
	}
	
	isx006_prev_af_mode = af_mode;

	return 0;
}


#define AF_POLL_COUNT_MAX  42
#define AF_POLL_DELAY_MS   50
int8_t  poll_cnt;

static long isx006_set_auto_focus(int af_mode)
{
	int32_t rc = 0;

    poll_cnt = 0;

	if (isx006_af_driver_ready_check() == 0)
	{
		return -EIO;
	}

	isx006_i2c_write(isx006_client->addr, 0x002E, 0x00, 8);
	isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	
	return rc;
}


#define AF_WINDOW_HSIZE 0x0250
#define AF_WINDOW_VSIZE 0x0250
#define AF_WINDOW_HSIZE_HALF 0x0128
#define AF_WINDOW_VSIZE_HALF 0x0128

static long isx006_set_auto_focus_position(int af_mode, int af_pos_x, int af_pos_y, int preview_dx, int preview_dy)
{
	int32_t rc = 0;
	long af_pos_start_x = 0, af_pos_start_y = 0;
	int AF_OPD4_HDELAY = 0, AF_OPD4_VDELAY = 0;
	int preview_dx_4x3_ratio = 0, preview_dy_4x3_ratio = 0;
	int H_ratio = 0, V_ratio = 0;
	int overflow_x = 0, overflow_y = 0;
	
	if (isx006_af_driver_ready_check() == 0)
	{
		return -EIO;
	}

	preview_dy_4x3_ratio = preview_dx * 3 / 4;

	if (preview_dy == preview_dy_4x3_ratio)
	{
		H_ratio = 2592 * 100 / preview_dx;
		V_ratio = 1944 * 100 / preview_dy;
		af_pos_start_x = af_pos_x * H_ratio - AF_WINDOW_HSIZE_HALF * 100;
		af_pos_start_y = af_pos_y * V_ratio - AF_WINDOW_VSIZE_HALF * 100;
	}
	else if (preview_dy < preview_dy_4x3_ratio)
	{
		H_ratio = 2592 * 100 / preview_dx;
		V_ratio = 1944 * 100 / preview_dy_4x3_ratio;
		af_pos_start_x = af_pos_x * H_ratio - AF_WINDOW_HSIZE_HALF * 100;
		af_pos_start_y = (af_pos_y + (preview_dy_4x3_ratio - preview_dy) / 2) * V_ratio - AF_WINDOW_VSIZE_HALF * 100;
	}
	else if (preview_dy > preview_dy_4x3_ratio)
	{
		preview_dx_4x3_ratio = preview_dx * 4 / 3;
		H_ratio = 2592 * 100 / preview_dx_4x3_ratio;
		V_ratio = 1944 * 100 / preview_dy;
		af_pos_start_x = (af_pos_x + (preview_dx_4x3_ratio - preview_dx) / 2) * H_ratio - AF_WINDOW_HSIZE_HALF * 100;
		af_pos_start_y = af_pos_y * V_ratio - AF_WINDOW_VSIZE_HALF * 100;
	}

	af_pos_start_x = af_pos_start_x / 100;
	af_pos_start_y = af_pos_start_y / 100;

	if (af_pos_start_x < 0)	af_pos_start_x = 0;
	if (af_pos_start_y < 0)	af_pos_start_y = 0;

	AF_OPD4_HDELAY = af_pos_start_x + 8 + 41;
	AF_OPD4_VDELAY = af_pos_start_y + 4;

	overflow_x = (AF_OPD4_HDELAY + AF_WINDOW_HSIZE) - 2608;
	overflow_y = (AF_OPD4_VDELAY + AF_WINDOW_VSIZE) - 1944;
	if (overflow_x >= 0) AF_OPD4_HDELAY = AF_OPD4_HDELAY - (overflow_x + 1);
	if (overflow_y >= 0) AF_OPD4_VDELAY = AF_OPD4_VDELAY - (overflow_y + 1);

	isx006_i2c_write(isx006_client->addr, 0x4C4C, AF_OPD4_HDELAY, 16); // AF_OPD4_HDELAY = X position + 8(margin)+41(ISX006 restriction)
	isx006_i2c_write(isx006_client->addr, 0x4C4E, AF_OPD4_VDELAY, 16); // AF_OPD4_VDELAY = Y position + 4(ISX006 restriction)
	isx006_i2c_write(isx006_client->addr, 0x4C50, AF_WINDOW_HSIZE, 16); // AF_OPD4_HVALID (H Size)
	isx006_i2c_write(isx006_client->addr, 0x4C52, AF_WINDOW_VSIZE, 16); // AF_OPD4_VVALID (V Size)

	rc = isx006_set_auto_focus(af_mode);
	
	return rc;
}


static long isx006_set_auto_focus_polling(void)
{
	int32_t rc = 0;
	unsigned short rdata = 0;

    poll_cnt++;
    
	if (poll_cnt < AF_POLL_COUNT_MAX)
	{
		isx006_i2c_read(isx006_client->addr, 0x6D76, &rdata, 8);
		if (rdata != 0x08)
		{
			rc = -1;
		}
		else
		{
        	rc = 0;
		}
	}
	else
	{
        rc = 0;
	}

	return rc;
}


static long isx006_get_auto_focus_result(void)
{
	int32_t rc = 0;
	unsigned short rdata1 = 0;

    if (poll_cnt == AF_POLL_COUNT_MAX)
    {
		isx006_i2c_write(isx006_client->addr, 0x4885, 0x01, 8);
		rc = -1;
    }
    else
    {
    	isx006_i2c_write(isx006_client->addr, 0x00FC, 0x10, 8);
    	isx006_i2c_read(isx006_client->addr, 0x6D77, &rdata1, 8);
    	if (rdata1 != 0x01)
    	{
    		rc = -1;
    	}
    	else
    	{
    		rc = 0;
    	}

    	isx006_i2c_write(isx006_client->addr, 0x002E, 0x03, 8);
    	isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
    }

    return rc;
}


static long isx006_set_af_cancel(void)
{
	isx006_i2c_write(isx006_client->addr, 0x4885, 0x01, 8);

	return 0;
}


static long isx006_set_reflect(int reflect)
{
	long rc;

	if (isx006_prev_reflect == reflect)
		return 0;

	if (reflect < 0 || reflect > 3)
	{
		return -EINVAL;
	}

	rc = isx006_i2c_write_table(isx006_regs.reflect_tbl + isx006_regs.reflect_tbl_size * reflect, isx006_regs.reflect_tbl_size);

	if (rc < 0)
		return rc;

	isx006_prev_reflect = reflect;

	return 0;
}


static long isx006_set_jpeg_quality(int quality)
{
	if (isx006_prev_jpeg_quality == quality)
		return 0;

	if (quality <= 0 || quality > 100)
	{
		return -EINVAL;
	}

	if (quality > 60 && quality <= 70)
	{
		isx006_i2c_write(isx006_client->addr, 0x0204, 0x00, 8);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	}
	else if (quality > 70 && quality <= 80)
	{
		isx006_i2c_write(isx006_client->addr, 0x0204, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	}
	else if (quality > 80)
	{
		isx006_i2c_write(isx006_client->addr, 0x0204, 0x02, 8);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	}
	else
	{
		SDBG(">> unsupported jpeg quality value (%d)\n", quality);	
	}
		
	isx006_prev_jpeg_quality = quality;

	return 0;
}


static long isx006_set_720p_mode(int vout_size)
{
	if (isx006_prev_vout_size == vout_size)
		return 0;

	if (vout_size == 720)
	{
		isx006_i2c_write(isx006_client->addr, 0x01BD, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x0103, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x0022, 0x0500, 16);
		isx006_i2c_write(isx006_client->addr, 0x0028, 0x02D0, 16);
		isx006_i2c_write(isx006_client->addr, 0x0383, 0x02, 8);
		isx006_i2c_write(isx006_client->addr, 0x02A4, 0x0000, 16);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	}
	else
	{
		isx006_i2c_write(isx006_client->addr, 0x01BD, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x0103, 0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x0022, 0x0320, 16);
		isx006_i2c_write(isx006_client->addr, 0x0028, 0x0258, 16);
		isx006_i2c_write(isx006_client->addr, 0x0383, 0x02, 8);
		isx006_i2c_write(isx006_client->addr, 0x02A4, 0x0000, 16);
		isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
	}

    isx006_prev_vout_size = vout_size;

	return 0;
}


static long isx006_vt_init(void)
{
	return 0;
}


long isx006_esd_recover(void)
{
    return 0;
}


static long isx006_set_zoom(int value, int width, int height)
{
	long rc;
	int zoom_value;

	if (isx006_prev_zoom == value && isx006_prev_zoom_width == width && isx006_prev_zoom_height == height)
		return 0;

	if (value < 0 || value > 5)
	{
		return -EINVAL;
	}

    zoom_value = value;

	if (width > 1600 && height >= 1200)
	{
		if (width == 2560)
		{
            zoom_value = 0;
		}
		rc = isx006_i2c_write_table(isx006_regs.zoom_x1p27_tbl + isx006_regs.zoom_x1p27_tbl_size * zoom_value, isx006_regs.zoom_x1p27_tbl_size);
	}
	else
	{
		rc = isx006_i2c_write_table(isx006_regs.zoom_x1p62_tbl + isx006_regs.zoom_x1p62_tbl_size * zoom_value, isx006_regs.zoom_x1p62_tbl_size);
	}

	if (rc < 0)
		return rc;

	isx006_prev_zoom = value;
	isx006_prev_zoom_width = width;
	isx006_prev_zoom_height = height;

	return 0;

}


static long isx006_wait_change_mode(void)
{
    int32_t loop_count = 200;
    unsigned short rdata = 0;
    int8_t cm_changed = 0;
    
    while (loop_count > 0)
	{
		isx006_i2c_read(isx006_client->addr, 0x00F8, &rdata, 8);
       	cm_changed = (int8_t)((rdata >> 1) & 0x0001);

        if (cm_changed == 1)
	    {
			do
			{
				isx006_i2c_write(isx006_client->addr, 0x00FC, 0x02, 8);
	    		isx006_i2c_read(isx006_client->addr, 0x00F8, &rdata, 8);
				cm_changed = (int8_t)((rdata >> 1) & 0x0001);
			} while (cm_changed == 1);
           	return 0;
        }
		
        mdelay(10);
        loop_count--;
    }

    return -EIO;
}


static long isx006_wait_jpegupdate_done(void)
{
    int32_t loop_count = 200;
	unsigned short rdata = 0;
    int8_t jpeg_update = 0;
	int8_t jpeg_status = 0;

    while (loop_count > 0)
    {
		isx006_i2c_read(isx006_client->addr, 0x00F8, &rdata, 8);
       	jpeg_update = (int8_t)((rdata >> 2) & 0x0001);

        if (jpeg_update == 1)
    	{
			do
			{
	  			isx006_i2c_write(isx006_client->addr, 0x00FC, 0x04, 8);
  				isx006_i2c_read(isx006_client->addr, 0x00F8, &rdata, 8);
				jpeg_update = (int8_t)((rdata >> 2) & 0x0001);
			} while (jpeg_update == 1);

		    isx006_i2c_read(isx006_client->addr, 0x0200, &rdata, 8);
		    jpeg_status = (int8_t)(rdata & 0x0003);
			if (jpeg_status == 0)
			{
				isx006_i2c_read(isx006_client->addr, 0x5A01, &rdata, 8);
				return 0;
			}
        }

        mdelay(10);
        loop_count--;
    }

    return -EIO;
}


#if defined(CONFIG_SKTS_CAM_KERNEL_COMMON)
static long isx006_set_sensor_mode(int mode, struct jpeg_capture_cfg jpeg_capture_info)
#else
static long isx006_set_sensor_mode(int mode)
#endif
{
	int32_t rc = -EIO;
	int32_t final_rc = 0;
	unsigned short rdata = 0;

#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_MAIN)
	g_enable_i2c_klog_main = 0;
#else
	g_enable_i2c_klog_main = 1;
#endif

	if (isx006_prev_sensor_mode == mode)
		return 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
        rc = isx006_reg_init_2();
		if (rc < 0)
		{
			return rc;
		}
#endif
		rc = isx006_i2c_write_table(&isx006_regs.preview_tbl[0], isx006_regs.preview_tbl_size);
		if (rc < 0)
		{
			return rc;
		}

#if defined(CONFIG_SKTS_CAM_KERNEL_COMMON)
		do {
			mdelay(10);
			isx006_i2c_read(isx006_client->addr, 0x0004, &rdata, 8);
		} while ( rdata & 0x03);
		isx006_i2c_write(isx006_client->addr, 0x00FC, 0x02, 8);

        if (isx006_snapshot_done == 1)
        {
            isx006_snapshot_done = 0;
            break;
        }

		switch (isx006_prev_af_mode)
		{
		case 0:
		case 2:
		case 3:
			isx006_i2c_write(isx006_client->addr, 0x002E, 0x02, 8);
			isx006_i2c_write(isx006_client->addr, 0x4852, 0x0082, 16);
			isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
			isx006_i2c_write(isx006_client->addr, 0x4850, 0x01, 8);
			break;
		case 1:
			isx006_i2c_write(isx006_client->addr, 0x002E, 0x02, 8);
			isx006_i2c_write(isx006_client->addr, 0x4852, 0x0242, 16);
			isx006_i2c_write(isx006_client->addr, 0x0012, 0x01, 8);
			isx006_i2c_write(isx006_client->addr, 0x4850, 0x01, 8);
			break;
		}		
#endif
		break;
			
	case SENSOR_SNAPSHOT_MODE:
		isx006_i2c_write(isx006_client->addr, 0x00FC,   0x1F, 8);
		
		if ((rc = isx006_i2c_write_table(&isx006_regs.snapshot_yuv_tbl[0], isx006_regs.snapshot_yuv_tbl_size))) final_rc = rc;
		if ((rc = isx006_wait_change_mode())) final_rc = rc;
		mdelay(100);

		if (final_rc < 0)
		{
			return final_rc;
		}
		isx006_snapshot_done = 1;
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		isx006_i2c_write(isx006_client->addr, 0x00FC,   0x1F, 8);
		
		isx006_i2c_write(isx006_client->addr, 0x021A, jpeg_capture_info.jpeg_capture_yuv_tn_width,  16);
		isx006_i2c_write(isx006_client->addr, 0x021C, jpeg_capture_info.jpeg_capture_yuv_tn_height, 16);
		isx006_i2c_write(isx006_client->addr, 0x0224,   0x01, 8);
		isx006_i2c_write(isx006_client->addr, 0x02C8,   0x00, 8);
		isx006_i2c_write(isx006_client->addr, 0x001D,   0x1B, 8);
		isx006_i2c_write(isx006_client->addr, 0x0024, jpeg_capture_info.jpeg_capture_width,  16);
		isx006_i2c_write(isx006_client->addr, 0x002A, jpeg_capture_info.jpeg_capture_height, 16);
		isx006_i2c_write(isx006_client->addr, 0x0011,   0x02, 8);

		if ((rc = isx006_wait_change_mode())) final_rc = rc;
		mdelay(100);
		if ((rc = isx006_wait_jpegupdate_done())) final_rc = rc;

		if (final_rc < 0)
		{
			return final_rc;
		}
		isx006_snapshot_done = 1;
		break;

	default:
		return -EINVAL;
	}

	isx006_prev_sensor_mode = mode;

	return 0;
}


static int isx006_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	rc = isx006_reset(data);
	if (rc < 0) {
		CDBG("reset failed!\n");
		goto init_probe_fail;
	}

	rc = isx006_reg_init();
	if (rc < 0)
		goto init_probe_fail;

	return rc;

init_probe_fail:
	return rc;
}


int isx006_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	isx006_ctrl = kzalloc(sizeof(struct isx006_ctrl), GFP_KERNEL);
	if (!isx006_ctrl) {
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		isx006_ctrl->sensordata = data;

	msm_camio_camif_pad_reg_reset();

	rc = isx006_sensor_init_probe(data);
	if (rc < 0) {
		CDBG("isx006_sensor_init failed!\n");
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	kfree(isx006_ctrl);
	return rc;
}


static int isx006_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&isx006_wait_queue);
	return 0;
}


int isx006_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,	sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&isx006_sem); */

#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_MAIN)
	g_enable_i2c_klog_main = 0;
#else
	g_enable_i2c_klog_main = 1;
#endif

	switch (cfg_data.cfgtype)
	{
	case CFG_SET_MODE:
		if (cfg_data.mode == SENSOR_PREVIEW_MODE)
			SDBG("isx006_sensor_config : CFG_SET_MODE,          mode = SENSOR_PREVIEW_MODE\n");
		else if (cfg_data.mode == SENSOR_SNAPSHOT_MODE)
			SDBG("isx006_sensor_config : CFG_SET_MODE,          mode = SENSOR_SNAPSHOT_MODE\n");
		else if (cfg_data.mode == SENSOR_RAW_SNAPSHOT_MODE)
			SDBG("isx006_sensor_config : CFG_SET_MODE,          mode = SENSOR_RAW_SNAPSHOT_MODE\n");
#if defined(CONFIG_SKTS_CAM_KERNEL_COMMON)
		rc = isx006_set_sensor_mode(cfg_data.mode, cfg_data.cfg.jpeg_capture_info);
#else
		rc = isx006_set_sensor_mode(cfg_data.mode);
#endif
		break;

	case CFG_SET_EFFECT:
		rc = isx006_set_effect(cfg_data.mode, cfg_data.cfg.effect);
		break;

	case CFG_SET_WB:
		rc = isx006_set_wb(cfg_data.cfg.wb);
		break;

	case CFG_SET_BRIGHTNESS:
		rc = isx006_set_brightness(cfg_data.cfg.brightness);
		break;

	case CFG_SET_EXPOSURE_MODE:
		rc = isx006_set_exposure_mode(cfg_data.cfg.exposure);
		break;

	case CFG_SET_ISO_MODE:
		rc = isx006_set_iso_mode(cfg_data.cfg.iso);
		break;

	case CFG_SET_SCENE_MODE:
		rc = isx006_set_scene_mode(cfg_data.cfg.scene_mode);
		break;

	case CFG_SET_FPS:
		rc = isx006_set_frame_rate(cfg_data.cfg.fixed_fps);
		break;

	case CFG_SET_AF_MODE:
		rc = isx006_set_af_mode(cfg_data.cfg.af_info.af_mode);
		break;

	case CFG_SET_AUTO_FOCUS:
		rc = isx006_set_auto_focus(cfg_data.cfg.af_info.af_mode);
		break;

	case CFG_SET_AUTO_FOCUS_POSITION:
		rc = isx006_set_auto_focus_position(cfg_data.cfg.af_info.af_mode, cfg_data.cfg.af_info.af_pos_x, cfg_data.cfg.af_info.af_pos_y,
			                                cfg_data.cfg.af_info.preview_dx, cfg_data.cfg.af_info.preview_dy);
		break;

    case CFG_SET_AUTO_FOCUS_POLLING:
		rc = isx006_set_auto_focus_polling();
		break;

    case CFG_GET_AUTO_FOCUS_RESULT:
		rc = isx006_get_auto_focus_result();
		break;

	case CFG_SET_AF_CANCEL:
		rc = isx006_set_af_cancel();
		break;

	case CFG_SET_REFLECT:
		rc = isx006_set_reflect(cfg_data.cfg.reflect);
		break;

	case CFG_SET_JPEG_QUALITY:
		rc = isx006_set_jpeg_quality(cfg_data.cfg.jpeg_quality);
		break;

	case CFG_SET_ZOOM:
		rc = isx006_set_zoom(cfg_data.cfg.ZoomInfo.zoom_value, cfg_data.cfg.ZoomInfo.picture_dx, cfg_data.cfg.ZoomInfo.picture_dy);
		break;

    case CFG_SET_720P_MODE:
        rc = isx006_set_720p_mode(cfg_data.cfg.vout_size);
		break;        

    case CFG_SET_VT_TUNE:
		rc = isx006_vt_init();
		break;

    case CFG_SET_ESD_RECOVER:
		rc = isx006_esd_recover();
		break;
    
	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = -EINVAL;
		break;
	}

	/* up(&isx006_sem); */

	return rc;
}


int isx006_sensor_release(void)
{
	int rc = 0;
	u32 cfg = 0;
	struct vreg *vreg_cam;

	/* down(&isx006_sem); */
	kfree(isx006_ctrl);
	/* up(&isx006_sem); */

	cfg = GPIO_CFG(S1_GPIO_CAM_CLK_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_CAM_CLK_EN, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_nRST, 0);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_nRST, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_EN, 0);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_EN, 0);	
	vreg_cam = vreg_get(NULL, "gp1"); vreg_disable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp6"); vreg_disable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp2"); vreg_disable(vreg_cam);
	
	mdelay(10);

	isx006_prev_wb = -1;
	isx006_prev_brightness = -1;
	isx006_prev_exposure_mode = -1;
	isx006_prev_iso_mode = -1;
	isx006_prev_scene_mode = 0;
	isx006_prev_ffps = -1;
	isx006_prev_sensor_mode = -1;
	isx006_af_driver_check_first_time = 1;
	isx006_af_driver_ready = 0;
	isx006_prev_af_mode = -1;
	isx006_prev_reflect = 0;
	isx006_prev_jpeg_quality = -1;
	isx006_prev_zoom = -1;
	isx006_prev_zoom_width = -1;
	isx006_prev_zoom_height = -1;
	isx006_prev_vout_size = 600;
	isx006_i2c_write_mode = ISX006_I2C_WRITE_MODE_NORMAL;
	isx006_i2c_sequential_write_first_time = 1;
	isx006_i2c_cnt = 0;
    isx006_second_init_tbl_written = 0;
    isx006_delayed_scene_mode_setting = 0;
    isx006_snapshot_done = 0;

    isx006_suspend_disable = 0;
    wake_unlock(&rearcam_wake_lock);

	return rc;
}


static int isx006_i2c_probe(struct i2c_client *client, 	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	isx006_sensorw =
		kzalloc(sizeof(struct isx006_work), GFP_KERNEL);

	if (!isx006_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, isx006_sensorw);
	isx006_init_client(client);
	isx006_client = client;

	return 0;

probe_failure:
	kfree(isx006_sensorw);
	isx006_sensorw = NULL;
	return rc;
}


static const struct i2c_device_id isx006_i2c_id[] = {
	{ "isx006", 0},
	{ },
};


static struct i2c_driver isx006_i2c_driver = {
	.id_table = isx006_i2c_id,
	.probe  = isx006_i2c_probe,
	.remove = __exit_p(isx006_i2c_remove),
	.driver = {
		.name = "isx006",
	},
};


static int isx006_sensor_probe(const struct msm_camera_sensor_info *info, struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&isx006_i2c_driver);

	if (rc < 0 )
	{
		rc = -ENOTSUPP;
		goto probe_done;
	}

	s->s_init    = isx006_sensor_init;
	s->s_release = isx006_sensor_release;
	s->s_config  = isx006_sensor_config;

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}


static int __isx006_probe(struct platform_device *pdev)
{
    wake_lock_init(&rearcam_wake_lock, WAKE_LOCK_SUSPEND, "rear_camera"); 

	return msm_camera_drv_start(pdev, isx006_sensor_probe);
}


static int __isx006_suspend(struct platform_device *pdev, pm_message_t state)
{
    if (isx006_suspend_disable == 0)
    {
        return 0;
    }
    else
    {
        return -EBUSY;
    }
}


static struct platform_driver msm_camera_driver = {
	.probe = __isx006_probe,
	.suspend = __isx006_suspend,
	.driver = {
		.name = "msm_camera_isx006",
		.owner = THIS_MODULE,
	},
};


static int __init isx006_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}


module_init(isx006_init);
