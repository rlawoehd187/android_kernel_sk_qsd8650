/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __MSM_BATTERY_H__
#define __MSM_BATTERY_H__

#define AC_CHG     0x00000001
#define USB_CHG    0x00000002

struct msm_psy_batt_pdata {
	u32 voltage_max_design;
	u32 voltage_min_design;
	u32 avail_chg_sources;
	u32 batt_technology;
	u32 (*calculate_capacity)(u32 voltage);
#ifdef CONFIG_MACH_QSD8X50_S1
	void (*config_gpio)(void);
#endif
};

#ifdef CONFIG_MACH_QSD8X50_S1
#define GET_VBATT_TABLE_SIZE()    ((u32)(VBATT_TABLE_SIZE))

typedef enum
{
    BATT_00_PERCENT  = 0,
    BATT_05_PERCENT  = 5,
    BATT_10_PERCENT  = 10,
    BATT_15_PERCENT  = 15,
    BATT_20_PERCENT  = 20,
    BATT_25_PERCENT  = 25,
    BATT_30_PERCENT  = 30,
    BATT_35_PERCENT  = 35,
    BATT_40_PERCENT  = 40,
    BATT_45_PERCENT  = 45,
    BATT_50_PERCENT  = 50,
    BATT_55_PERCENT  = 55,
    BATT_60_PERCENT  = 60,
    BATT_65_PERCENT  = 65,
    BATT_70_PERCENT  = 70,
    BATT_75_PERCENT  = 75,
    BATT_80_PERCENT  = 80,
    BATT_85_PERCENT  = 85,
    BATT_90_PERCENT  = 90,
    BATT_95_PERCENT  = 95,
    BATT_100_PERCENT = 100,
    VBATT_TABLE_SIZE = 21
} vbatt_capacity_type ;

typedef struct 
{
    vbatt_capacity_type vbatt_capacity_percentage;
    u32                 vbatt_translation_voltage;
} vbatt_capacity_table_type ;
#endif /* CONFIG_MACH_QSD8X50_S1 */
#endif
