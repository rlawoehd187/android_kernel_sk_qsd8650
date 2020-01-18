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

#ifndef MT9V113_H
#define MT9V113_H

#include <linux/types.h>
#include <mach/camera.h>


#define FEATURE_SKTS_CAM_SENSOR_FIX
#define FEATURE_SKTS_CAM_SENSOR_REFRESH_POLLING
//#define FEATURE_SKTS_CAM_SENSOR_ESD_RECOVER
#define FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_SUB


extern struct mt9v113_reg mt9v113_regs;

struct mt9v113_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

struct mt9v113_reg {
	const struct mt9v113_i2c_reg_conf *init_tbl;
	uint16_t init_tbl_size;
	const struct mt9v113_i2c_reg_conf *init_vt_tbl;
	uint16_t init_vt_tbl_size;
	const struct mt9v113_i2c_reg_conf *preview_tbl;
	uint16_t preview_tbl_size;
	const struct mt9v113_i2c_reg_conf *snapshot_tbl;
	uint16_t snapshot_tbl_size;
	const struct mt9v113_i2c_reg_conf *wb_tbl;
	uint16_t wb_tbl_size;
	const struct mt9v113_i2c_reg_conf *brightness_tbl;
	uint16_t brightness_tbl_size;
	const struct mt9v113_i2c_reg_conf *exposure_tbl;
	uint16_t exposure_tbl_size;
	const struct mt9v113_i2c_reg_conf *reflect_tbl;
	uint16_t reflect_tbl_size;
	const struct mt9v113_i2c_reg_conf *fixed_fps_tbl;
	uint16_t fixed_fps_tbl_size;	
};

#endif /* MT9V113_H */
