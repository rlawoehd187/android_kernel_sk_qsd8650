/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __LINUX_I2C_BQ27505_H__
#define __LINUX_I2C_BQ27505_H__

typedef enum {
	BQ27505_STD_CNTL  = 0x00,
	BQ27505_STD_TEMP  = 0x06, /* Temperature, 0.1K, NA in SK-S100 */
	BQ27505_STD_VOLT  = 0x08, /* Voltage, mV */
	BQ27505_STD_FLAGS = 0x0A,
	BQ27505_STD_NAC   = 0x0C, /* NominalAvailableCapacity, mAh */
	BQ27505_STD_FAC   = 0x0E, /* FullAvailableCapacity, mAh */
	BQ27505_STD_RM    = 0x10, /* RemainingCapacity, mAh */
	BQ27505_STD_FCC   = 0x12, /* FullChargeCapacity, mAh */
	BQ27505_STD_AI    = 0x14, /* AverageCurrent, mA */
	BQ27505_STD_TTE   = 0x16, /* TimeToEmpty, Minutes */
	BQ27505_STD_TTF   = 0x18, /* TimeToFull, Minutes */
	BQ27505_STD_SOH   = 0x28, /* StateOfHealth, %/num */
	BQ27505_STD_SOC   = 0x2C, /* StateOfCharge, % */
	BQ27505_STD_NIC   = 0x2E  /* NormalizedImpedanceCall, mohm */
} bq27505_std_cmd;

typedef enum {
	BQ27505_SUB_CONTROL_STATUS  = 0x0000,
	BQ27505_SUB_DEVICE_TYPE     = 0x0001,
	BQ27505_SUB_FW_VERSION      = 0x0002,
	BQ27505_SUB_HW_VERSION      = 0x0003,
	BQ27505_SUB_SET_HIBERNATE   = 0x0011,
	BQ27505_SUB_CLEAR_HIBERNATE = 0x0012,
	BQ27505_SUB_ENABLE_SLEEP    = 0x0013,
	BQ27505_SUB_DISABLE_SLEEP   = 0x0014,
	BQ27505_SUB_IT_ENABLE       = 0x0021,
	BQ27505_SUB_RESET           = 0x0041
} bq27505_sub_cmd;

struct bq27505_platform_data {
	int pwr_pin;
	int (*init_gpio_cfg)(void);
};


#ifdef CONFIG_BATTERY_BQ27505
extern bool bq27505_battery_probed(void);

extern int bq27505_battery_property(bq27505_std_cmd cmd, int *value);

extern void bq27505_battery_interrupt(bool enable);
#else
#define bq27505_battery_probed()      (false)

#define bq27505_battery_property(cmd, value) (0)

#define bq27505_battery_interrupt(enable)
#endif

#endif /* __LINUX_I2C_BQ27505_H__ */
