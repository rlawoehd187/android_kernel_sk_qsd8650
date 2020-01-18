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
#include "mt9v113.h"
#include "camsensor_tune.h"
#if defined(CONFIG_SKTS_CAM_KERNEL_COMMON)
#include <linux/proc_fs.h>
#endif
#include <linux/wakelock.h>


#define MT9V113_REFRESH_CHECK_RETRY_CNT 40
#define MT9V113_REFRESH_DONE 0
#define MT9V113_REFRESH_POLL_TIME 50

struct mt9v113_work {
	struct work_struct work;
};

static struct  mt9v113_work *mt9v113_sensorw;
static struct  i2c_client *mt9v113_client;

struct mt9v113_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};

static struct mt9v113_ctrl *mt9v113_ctrl;

static struct wake_lock frontcam_wake_lock;

static int g_enable_i2c_klog_sub = 0;

static DECLARE_WAIT_QUEUE_HEAD(mt9v113_wait_queue);
DECLARE_MUTEX(mt9v113_sem);

static int mt9v113_open_count = 0;
static int mt9v113_prev_wb = -1;
static int mt9v113_prev_brightness = -1;
static int mt9v113_prev_exposure_mode = -1;
static int mt9v113_prev_ffps = -1;
static int mt9v113_prev_sensor_mode = -1;
static int mt9v113_prev_reflect = 0;
#if defined(FEATURE_SKTS_CAM_SENSOR_ESD_RECOVER)
static int mt9v113_is_in_esd_recover = 0;
#endif
static int mt9v113_use_vt = 0;
static int mt9v113_suspend_disable = 0;


/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9v113_reg mt9v113_regs;


/*=============================================================*/

static int mt9v113_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
	u32 cfg = 0;
	struct vreg *vreg_cam;

	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_nRST, 1);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_EN, 0);
	
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_nRST, 1);

	mdelay(10);

	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_EN, 1);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_EN, 1);
	vreg_cam = vreg_get(NULL, "gp1"); vreg_set_level(vreg_cam, 2600); vreg_enable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp6"); vreg_set_level(vreg_cam, 2800); vreg_enable(vreg_cam);
	vreg_cam = vreg_get(NULL, "gp2"); vreg_set_level(vreg_cam, 2800); vreg_enable(vreg_cam);

	mdelay(10);
	
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_nRST, 0);
	cfg = GPIO_CFG(S1_GPIO_CAM_CLK_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);	gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_CAM_CLK_EN, 1);
	
	mdelay(10);

	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_nRST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_nRST, 1);

	mdelay(30);

	return rc;
}


static int mt9v113_i2c_rxdata(unsigned short saddr,
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

	if (i2c_transfer(mt9v113_client->adapter, msgs, 2) < 0) {
		return -EIO;
	}

	return 0;
}


static int32_t mt9v113_i2c_read(unsigned short saddr, unsigned short raddr, unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];
	int8_t retry_cnt = 0;

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = mt9v113_i2c_rxdata(saddr, buf, 2);
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
		*rdata = buf[0] << 8 | buf[1];
	}

	return rc;
}


static int32_t mt9v113_i2c_txdata(unsigned short saddr, unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(mt9v113_client->adapter, msg, 1) < 0) {
		return -EIO;
	}

	return 0;
}


static int32_t mt9v113_i2c_write(unsigned short saddr, unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	int8_t retry_cnt = 0;
	int8_t poll_cnt = 0;
	uint16_t read_data;

	if (waddr == 0 && wdata == 0)
	{
		return 0;
	}

	if (waddr == 0x00FF)
	{
		mdelay(wdata);
		return 0;
	}

    if (waddr == 0xFFFF)
    {
    	for (poll_cnt = 0; poll_cnt < 10; poll_cnt++)
    	{
    		mdelay(10);
    		mt9v113_i2c_read(mt9v113_client->addr, 0x0018, &read_data);
    		if(read_data == 0x002A)
    			break;
    	}
    	if (poll_cnt == 10)
    	{
    	    SDBG("polling refresh(0) fail! (poll time : %3d ms)\n", 10 * poll_cnt);
    	    return -EFAULT;
    	}

    	return 0;
    }

	memset(buf, 0, sizeof(buf));

	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	for (retry_cnt = 0; retry_cnt < 3; retry_cnt++)
	{
		rc = mt9v113_i2c_txdata(saddr, buf, 4);
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


static int32_t mt9v113_i2c_write_table(struct mt9v113_i2c_reg_conf const *reg_conf_tbl, int num_of_items_in_table)
{
	int i;
	int32_t rc = 0;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9v113_i2c_write(mt9v113_client->addr, reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}


static long mt9v113_reg_init(void)
{
	long rc;
#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
	int8_t poll_cnt;
	uint16_t state;
#endif
#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
    struct mt9v113_i2c_reg_conf *tune_init_tbl = NULL;
    uint16_t tune_init_tbl_size;
#endif

    wake_lock(&frontcam_wake_lock);
    mt9v113_suspend_disable = 1;
	
	g_enable_i2c_klog_sub = 0;
	
#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)
	rc = camsensor_set_file_read(MT9V113_INIT_REG_FILE, (uint32_t *)&tune_init_tbl, &tune_init_tbl_size);
	if (rc < 0)
	{
		if (mt9v113_use_vt == 0)
		{
	    	rc = mt9v113_i2c_write_table(&mt9v113_regs.init_tbl[0], mt9v113_regs.init_tbl_size);
    	}
    	else
    	{
		    rc = mt9v113_i2c_write_table(&mt9v113_regs.init_vt_tbl[0], mt9v113_regs.init_vt_tbl_size);
 	    }
	}
	else
	{
		rc = mt9v113_i2c_write_table((struct mt9v113_i2c_reg_conf *)tune_init_tbl, tune_init_tbl_size);
	}
#else
	if (mt9v113_use_vt == 0)
	{
    	rc = mt9v113_i2c_write_table(&mt9v113_regs.init_tbl[0], mt9v113_regs.init_tbl_size);
	}
	else
	{
	    rc = mt9v113_i2c_write_table(&mt9v113_regs.init_vt_tbl[0], mt9v113_regs.init_vt_tbl_size);
    }
    if (rc < 0)
        return rc;
#endif

#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
	g_enable_i2c_klog_sub = 0;
#else
	g_enable_i2c_klog_sub = 1;
#endif
	mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);
	mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0006);
	for (poll_cnt = 0; poll_cnt < MT9V113_REFRESH_CHECK_RETRY_CNT; poll_cnt++)
	{
		mdelay(MT9V113_REFRESH_POLL_TIME);
		mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);	
		mt9v113_i2c_read(mt9v113_client->addr, 0x0990, &state);
		if(state == MT9V113_REFRESH_DONE)
			break;
	}
	if (poll_cnt == MT9V113_REFRESH_CHECK_RETRY_CNT)
	    SDBG("polling refresh(1) fail! (poll time : %3d ms)\n", MT9V113_REFRESH_POLL_TIME * poll_cnt);

	mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);
	mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0005);
	for (poll_cnt = 0; poll_cnt < MT9V113_REFRESH_CHECK_RETRY_CNT; poll_cnt++)
	{
		mdelay(MT9V113_REFRESH_POLL_TIME);
		mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);	
		mt9v113_i2c_read(mt9v113_client->addr, 0x0990, &state);
		if(state == MT9V113_REFRESH_DONE)
			break;
	}
	if (poll_cnt == MT9V113_REFRESH_CHECK_RETRY_CNT)
	    SDBG("polling refresh(2) fail! (poll time : %3d ms)\n", MT9V113_REFRESH_POLL_TIME * poll_cnt);
#endif

	if (rc < 0)
		return rc;

	return 0;
}


static long mt9v113_set_effect(int mode, int effect)
{
	return 0;
}


static long mt9v113_set_wb(int wb)
{
	long rc;
	uint16_t wb_sel;

	if (mt9v113_prev_wb == wb)
		return 0;

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

	rc = mt9v113_i2c_write_table(mt9v113_regs.wb_tbl + mt9v113_regs.wb_tbl_size * wb_sel, mt9v113_regs.wb_tbl_size);
	
	if (rc < 0)
		return rc;

	mt9v113_prev_wb = wb;

	return 0;
}


static long mt9v113_set_brightness(int brightness)
{
	long rc;
#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
	int8_t poll_cnt;
	uint16_t state;
#endif

	if (mt9v113_prev_brightness == brightness)
		return 0;

	if (brightness > 6 || brightness < 0)
	{
		return -EINVAL;
	}
	
	rc = mt9v113_i2c_write_table(mt9v113_regs.brightness_tbl + mt9v113_regs.brightness_tbl_size * brightness, mt9v113_regs.brightness_tbl_size);

#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
	g_enable_i2c_klog_sub = 0;
#else
	g_enable_i2c_klog_sub = 1;
#endif
	mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);
	mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0005);
	for (poll_cnt = 0; poll_cnt < MT9V113_REFRESH_CHECK_RETRY_CNT; poll_cnt++)
	{
		mdelay(MT9V113_REFRESH_POLL_TIME);
		mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);	
		mt9v113_i2c_read(mt9v113_client->addr, 0x0990, &state);
		if(state == MT9V113_REFRESH_DONE)
			break;
	}
	if (poll_cnt == MT9V113_REFRESH_CHECK_RETRY_CNT)
	    SDBG("polling refresh fail! (poll time : %3d ms)\n", MT9V113_REFRESH_POLL_TIME * poll_cnt);
#endif

	if (rc < 0)
		return rc;

	mt9v113_prev_brightness = brightness;

	return 0;
}


static long mt9v113_set_exposure_mode(int exposure_mode)
{
	long rc;
#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
	int8_t poll_cnt;
	uint16_t state;
#endif

	if (mt9v113_prev_exposure_mode == exposure_mode)
		return 0;	

	if (exposure_mode < 0 || exposure_mode > 2)
	{
		return -EINVAL;
	}
	
	rc = mt9v113_i2c_write_table(mt9v113_regs.exposure_tbl + mt9v113_regs.exposure_tbl_size * exposure_mode, mt9v113_regs.exposure_tbl_size);

#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
	g_enable_i2c_klog_sub = 0;
#else
	g_enable_i2c_klog_sub = 1;
#endif
	mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);
	mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0005);
	for (poll_cnt = 0; poll_cnt < MT9V113_REFRESH_CHECK_RETRY_CNT; poll_cnt++)
	{
		mdelay(MT9V113_REFRESH_POLL_TIME);
		mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);	
		mt9v113_i2c_read(mt9v113_client->addr, 0x0990, &state);
		if(state == MT9V113_REFRESH_DONE)
			break;
	}
	if (poll_cnt == MT9V113_REFRESH_CHECK_RETRY_CNT)
	    SDBG("polling refresh fail! (poll time : %3d ms)\n", MT9V113_REFRESH_POLL_TIME * poll_cnt);
#endif

	if (rc < 0)
		return rc;

	mt9v113_prev_exposure_mode = exposure_mode;

	return 0;
}


static long mt9v113_set_frame_rate(int ffps)
{
	long rc;
	uint16_t ffps_sel;
#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
	int8_t poll_cnt;
	uint16_t state;
#endif

	if (mt9v113_prev_ffps == ffps)
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
		mt9v113_prev_ffps = ffps;
		return 0;
	}

    rc = mt9v113_i2c_write_table(mt9v113_regs.fixed_fps_tbl + mt9v113_regs.fixed_fps_tbl_size * ffps_sel, mt9v113_regs.fixed_fps_tbl_size);
    
#if defined(FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING)
#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
   	g_enable_i2c_klog_sub = 0;
#else
    g_enable_i2c_klog_sub = 1;
#endif
	mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);
	mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0006);
	for (poll_cnt = 0; poll_cnt < MT9V113_REFRESH_CHECK_RETRY_CNT; poll_cnt++)
	{
		mdelay(MT9V113_REFRESH_POLL_TIME);
		mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103);	
		mt9v113_i2c_read(mt9v113_client->addr, 0x0990, &state);
		if(state == MT9V113_REFRESH_DONE)
			break;
	}
	if (poll_cnt == MT9V113_REFRESH_CHECK_RETRY_CNT)
	    SDBG("polling refresh fail! (poll time : %3d ms)\n", MT9V113_REFRESH_POLL_TIME * poll_cnt);
#endif

	if (rc < 0)
	    return rc;

	mt9v113_prev_ffps = ffps;

	return 0;
}


static long mt9v113_set_reflect(int reflect)
{
	long rc;

	if (mt9v113_prev_reflect == reflect)
		return 0;

	if (reflect < 0 || reflect > 3)
	{
		return -EINVAL;
	}

	rc = mt9v113_i2c_write_table(mt9v113_regs.reflect_tbl + mt9v113_regs.reflect_tbl_size * reflect, mt9v113_regs.reflect_tbl_size);

	if (rc < 0)
		return rc;

	mt9v113_prev_reflect = reflect;

	return 0;
}


#if defined(FEATURE_SKTS_CAM_SENSOR_ESD_RECOVER)
long mt9v113_esd_recover(void)
{
    long rc1 = 0, rc2 = 0;

    mt9v113_is_in_esd_recover = 1;

    rc2 = mt9v113_reg_init();

    if (rc1 < 0 || rc2 < 0)
        SDBG("failed to esd recover! (rc1 = %ld, rc2 = %ld)\n", rc1, rc2);
    else
        SDBG("succeeded to esd recover!\n");

    mt9v113_is_in_esd_recover = 0;

    return 0;
}


long mt9v113_get_esd_recover_state(void)
{
    if (mt9v113_is_in_esd_recover == 1)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
#endif


static long mt9v113_set_sensor_mode(int mode)
{
	long rc = 0;

#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
	g_enable_i2c_klog_sub = 0;
#else
	g_enable_i2c_klog_sub = 1;
#endif

	if (mt9v113_prev_sensor_mode == mode)
		return 0;
	
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9v113_i2c_write_table(&mt9v113_regs.preview_tbl[0], mt9v113_regs.preview_tbl_size);
		if (rc < 0)
			return rc;
		break;
			
	case SENSOR_SNAPSHOT_MODE:
		rc = mt9v113_i2c_write_table(&mt9v113_regs.snapshot_tbl[0], mt9v113_regs.snapshot_tbl_size);
		if (rc < 0)
			return rc;
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		break;

	default:
		return -EINVAL;
	}

	mt9v113_prev_sensor_mode = mode;

	return 0;
}


static int mt9v113_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	int8_t retry_cnt = 0;

	rc = mt9v113_reset(data);
	if (rc < 0) {
		goto init_probe_fail;
	}

	rc = mt9v113_reg_init();
    if (rc < 0)
    {
    	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++)
    	{
            mt9v113_reset(data);
            rc = mt9v113_reg_init();
            if (rc == 0)
                break;
    	}
    }

	if (rc < 0)
		goto init_probe_fail;

	return rc;

init_probe_fail:
	return rc;
}


int mt9v113_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	mt9v113_ctrl = kzalloc(sizeof(struct mt9v113_ctrl), GFP_KERNEL);
	if (!mt9v113_ctrl) {
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9v113_ctrl->sensordata = data;

	msm_camio_camif_pad_reg_reset();

	rc = mt9v113_sensor_init_probe(data);
	if (rc < 0) {
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	kfree(mt9v113_ctrl);
	return rc;
}


static int mt9v113_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9v113_wait_queue);
	return 0;
}


int mt9v113_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,	sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&mt9v113_sem); */

#if defined(FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB)
	g_enable_i2c_klog_sub = 0;
#else
	g_enable_i2c_klog_sub = 1;
#endif

	switch (cfg_data.cfgtype)
	{
	case CFG_SET_MODE:
		if (cfg_data.mode == SENSOR_PREVIEW_MODE)
			SDBG("mt9v113_sensor_config : CFG_SET_MODE,     mode = SENSOR_PREVIEW_MODE\n");
		else if (cfg_data.mode == SENSOR_SNAPSHOT_MODE)
			SDBG("mt9v113_sensor_config : CFG_SET_MODE,     mode = SENSOR_SNAPSHOT_MODE\n");
		else if (cfg_data.mode == SENSOR_RAW_SNAPSHOT_MODE)
			SDBG("mt9v113_sensor_config : CFG_SET_MODE,     mode = SENSOR_SNAPSHOT_MODE\n");
		rc = mt9v113_set_sensor_mode(cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		rc = mt9v113_set_effect(cfg_data.mode, cfg_data.cfg.effect);
		break;

	case CFG_SET_WB:
		rc = mt9v113_set_wb(cfg_data.cfg.wb);
		break;

	case CFG_SET_BRIGHTNESS:
		rc = mt9v113_set_brightness(cfg_data.cfg.brightness);
		break;

	case CFG_SET_EXPOSURE_MODE:
		rc = mt9v113_set_exposure_mode(cfg_data.cfg.exposure);
		break;

	case CFG_SET_FPS:
		rc = mt9v113_set_frame_rate(cfg_data.cfg.fixed_fps);
		break;

	case CFG_SET_REFLECT:
		rc = mt9v113_set_reflect(cfg_data.cfg.reflect);
		break;

#if defined(FEATURE_SKTS_CAM_SENSOR_ESD_RECOVER)
    case CFG_SET_ESD_RECOVER:
		rc = mt9v113_esd_recover();
		break;

    case CFG_GET_ESD_RECOVER_STATE:
		rc = mt9v113_get_esd_recover_state();
		break;
#endif

	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = -EINVAL;
		break;
	}

	/* up(&mt9v113_sem); */

	return rc;
}


int mt9v113_sensor_release(void)
{
	int rc = 0;
	u32 cfg = 0;
	struct vreg *vreg_cam;

	/* down(&mt9v113_sem); */
	kfree(mt9v113_ctrl);
	/* up(&mt9v113_sem); */

	cfg = GPIO_CFG(S1_GPIO_CAM_CLK_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_CAM_CLK_EN, 0);
	cfg = GPIO_CFG(S1_GPIO_MAIN_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_MAIN_CAM_STANDBY, 0);
	cfg = GPIO_CFG(S1_GPIO_SUB_CAM_STANDBY, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA); gpio_tlmm_config(cfg, GPIO_ENABLE);
	gpio_set_value(S1_GPIO_SUB_CAM_STANDBY, 0);
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

	mt9v113_prev_wb = -1;
	mt9v113_prev_brightness = -1;
	mt9v113_prev_exposure_mode = -1;
	mt9v113_prev_ffps = -1;
	mt9v113_prev_sensor_mode = -1;
	mt9v113_prev_reflect = 0;
#if defined(FEATURE_SKTS_CAM_SENSOR_ESD_RECOVER)
    mt9v113_is_in_esd_recover = 0;
#endif
    mt9v113_use_vt = 0;

    mt9v113_suspend_disable = 0;
    wake_unlock(&frontcam_wake_lock);
    
	return rc;
}


#if defined(CONFIG_SKTS_CAM_KERNEL_COMMON)
#define MT9V113_SENSOR_STRING 7

static int mt9v113_sensor_select(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char buf[MT9V113_SENSOR_STRING];
	int ret = 0;

	if(copy_from_user(&buf, buffer, MT9V113_SENSOR_STRING))
	{
		ret = -EFAULT;
		goto mt9v113_write_proc_failed;
	}

	if(strncmp(buf, "normal", strlen("normal")) == 0)
		mt9v113_use_vt = 0;
	else if(strncmp(buf, "vt", strlen("vt")) == 0)
		mt9v113_use_vt = 1;

mt9v113_write_proc_failed:
	return ret;
}
#endif


static int mt9v113_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9v113_sensorw =
		kzalloc(sizeof(struct mt9v113_work), GFP_KERNEL);

	if (!mt9v113_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9v113_sensorw);
	mt9v113_init_client(client);
	mt9v113_client = client;

	return 0;

probe_failure:
	kfree(mt9v113_sensorw);
	mt9v113_sensorw = NULL;
	return rc;
}


static const struct i2c_device_id mt9v113_i2c_id[] = {
	{ "mt9v113", 0},
	{ },
};


static struct i2c_driver mt9v113_i2c_driver = {
	.id_table = mt9v113_i2c_id,
	.probe  = mt9v113_i2c_probe,
	.remove = __exit_p(mt9v113_i2c_remove),
	.driver = {
		.name = "mt9v113",
	},
};


static int mt9v113_sensor_probe(const struct msm_camera_sensor_info *info, struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&mt9v113_i2c_driver);

	if (rc < 0 )
	{
		rc = -ENOTSUPP;
		goto probe_done;
	}

	s->s_init    = mt9v113_sensor_init;
	s->s_release = mt9v113_sensor_release;
	s->s_config  = mt9v113_sensor_config;

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}


static int __mt9v113_probe(struct platform_device *pdev)
{
    struct proc_dir_entry *mt9v113_entry;

	mt9v113_entry = create_proc_entry("mt9v113_select",	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, NULL);
	if (mt9v113_entry) {
		mt9v113_entry->read_proc = NULL;
		mt9v113_entry->write_proc = mt9v113_sensor_select;
		mt9v113_entry->data = NULL;
	}

    wake_lock_init(&frontcam_wake_lock, WAKE_LOCK_SUSPEND, "front_camera");

	return msm_camera_drv_start(pdev, mt9v113_sensor_probe);
}


static int __mt9v113_suspend(struct platform_device *pdev, pm_message_t state)
{
    if (mt9v113_suspend_disable == 0)
    {
        return 0;
    }
    else
    {
        return -EBUSY;
    }
}


static struct platform_driver msm_camera_driver = {
	.probe = __mt9v113_probe,
	.suspend = __mt9v113_suspend,
	.driver = {
		.name = "msm_camera_mt9v113",
		.owner = THIS_MODULE,
	},
};


static int __init mt9v113_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}


module_init(mt9v113_init);
