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

#ifndef ISX006_H
#define ISX006_H

#include <linux/types.h>
#include <mach/camera.h>


#define FEATURE_SKTS_CAM_SENSOR_FIX
#define FEATURE_SKTS_CAM_SENSOR_SEQUENTIAL_WRITE_MODE
#define FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE
#define FEATURE_SKTS_CAM_SENSOR_DEAD_PIXEL_CORRECT
#define FEATURE_SKTS_CAM_SENSOR_REDUCE_KLOG_MAIN


extern struct isx006_reg isx006_regs;

struct isx006_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	unsigned short width;
};

struct isx006_reg {
	const struct isx006_i2c_reg_conf *init_tbl;
	uint16_t init_tbl_size;
#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
	const struct isx006_i2c_reg_conf *init_tbl_2;
	uint16_t init_tbl_2_size;
#endif
#if defined(FEATURE_SKTS_CAM_SENSOR_DEAD_PIXEL_CORRECT)
	const struct isx006_i2c_reg_conf *dpc_ap001_tbl;
	uint16_t dpc_ap001_tbl_size;
	const struct isx006_i2c_reg_conf *dpc_ap003_tbl;
	uint16_t dpc_ap003_tbl_size;
#endif
	const struct isx006_i2c_reg_conf *preview_tbl;
	uint16_t preview_tbl_size;
	const struct isx006_i2c_reg_conf *snapshot_yuv_tbl;
	uint16_t snapshot_yuv_tbl_size;
	const struct isx006_i2c_reg_conf *wb_tbl;
	uint16_t wb_tbl_size;
	const struct isx006_i2c_reg_conf *brightness_tbl;
	uint16_t brightness_tbl_size;
	const struct isx006_i2c_reg_conf *exposure_tbl;
	uint16_t exposure_tbl_size;
	const struct isx006_i2c_reg_conf *iso_tbl;
	uint16_t iso_tbl_size;
	const struct isx006_i2c_reg_conf *scene_tbl;
	uint16_t scene_tbl_size;
	const struct isx006_i2c_reg_conf *reflect_tbl;
	uint16_t reflect_tbl_size;
	const struct isx006_i2c_reg_conf *zoom_x1p62_tbl;
	uint16_t zoom_x1p62_tbl_size;
	const struct isx006_i2c_reg_conf *zoom_x1p27_tbl;
	uint16_t zoom_x1p27_tbl_size;
	const struct isx006_i2c_reg_conf *fixed_fps_tbl;
	uint16_t fixed_fps_tbl_size;
};


#endif /* ISX006_H */
