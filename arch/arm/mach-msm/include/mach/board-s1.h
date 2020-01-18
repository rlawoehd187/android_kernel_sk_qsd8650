/* linux/arch/arm/mach-msm/board-s1.h
 * Copyright (C) 2007-2009 SKTelesys Corporation.
 * Author: TJ Kim <tjkim@sk-w.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#ifndef __ARCH_ARM_MACH_MSM_BOARD_S1_H
#define __ARCH_ARM_MACH_MSM_BOARD_S1_H

#include <mach/board.h>

#define S1_BOARD_VER_PT01   0
#define S1_BOARD_VER_PT02   1
#define S1_BOARD_VER_WS01   2
#define S1_BOARD_VER_ES00   3
#define S1_BOARD_VER_ES01   4
#define S1_BOARD_VER_ES02   5
#define S1_BOARD_VER_ES03   6
#define S1_BOARD_VER_PP     7


#define S1_PM_WIFI_SDIO_NAME    "mmc"
#define S1_PM_WIFI_SDIO_LEVEL   2850

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
#define S1_PM_BT_NAME           "gp3"
#define S1_PM_BT_LEVEL          2850
#else
#define S1_PM_WIFI_BT_CLK_NAME  "gp3"
#define S1_PM_WIFI_BT_CLK_LEVEL 1800
#endif

#define S1_PM_BT_CLK_BIT        0
#define S1_PM_WIFI_CLK_BIT      1


/* BT */
#define S1_GPIO_BT_UART_RTS     43
#define S1_GPIO_BT_UART_CTS     44
#define S1_GPIO_BT_UART_RXD     45
#define S1_GPIO_BT_UART_TXD     46

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
#define S1_GPIO_BT_REG_ON       32
#define S1_GPIO_BT_RST          134
#define S1_GPIO_BT_HOST_WAKE    117
#define S1_GPIO_BT_WAKE         118
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02)
#define S1_GPIO_BT_REG_ON       89
#define S1_GPIO_BT_HOST_WAKE    160
#define S1_GPIO_BT_WAKE         161
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#define S1_GPIO_BT_REG_ON       157
#define S1_GPIO_BT_HOST_WAKE    160
#define S1_GPIO_BT_WAKE         161
#endif


/* WIFI */
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
#define S1_GPIO_WIFI_REG_ON     26
#define S1_GPIO_WIFI_RST        27
#define S1_GPIO_WIFI_HOST_WAKE  125
#define S1_GPIO_WIFI_WAKE       126
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02)
#define S1_GPIO_WIFI_REG_ON     88
#define S1_GPIO_WIFI_HOST_WAKE  158
#define S1_GPIO_WIFI_WAKE       159
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
#define S1_GPIO_WIFI_REG_ON     140
#define S1_GPIO_WIFI_HOST_WAKE  158
#define S1_GPIO_WIFI_WAKE       159
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
#define S1_GPIO_WIFI_REG_ON     140
#define S1_GPIO_WIFI_HOST_WAKE  153
#define S1_GPIO_WIFI_WAKE       159
#endif

#define S1_GPIO_WIFI_CLK     62
#define S1_GPIO_WIFI_CMD     63
#define S1_GPIO_WIFI_D3      64
#define S1_GPIO_WIFI_D2      65
#define S1_GPIO_WIFI_D1      66
#define S1_GPIO_WIFI_D0      67

/* SD-MMC */
#define S1_GPIO_SD_D3        51
#define S1_GPIO_SD_D2        52
#define S1_GPIO_SD_D1        53
#define S1_GPIO_SD_D0        54
#define S1_GPIO_SD_CMD       55
#define S1_GPIO_SD_CLK       56

#define S1_GPIO_EN_SD_PWR    144
#define S1_GPIO_SD_nDET      82
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
#define S1_GPIO_TFLASH_EN    92
#endif

/* I2C */
#define S1_GPIO_AUDIO_SDA    36
#define S1_GPIO_AUDIO_SCL    37
#define S1_GPIO_CAM_DMB_SDA  40
#define S1_GPIO_CAM_DMB_SCL  41
#define S1_GPIO_SENSOR_SCL   28
#define S1_GPIO_SENSOR_SDA   29
#if (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_ES00)
#define S1_GPIO_TOUCH_SCL    95
#define S1_GPIO_TOUCH_SDA    96
#else
#define S1_GPIO_TOUCH_SDA    23
#define S1_GPIO_TOUCH_SCL    110
#define S1_GPIO_TPS_SCL      95
#define S1_GPIO_TPS_SDA      96
#endif

/* KEY */
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
#define S1_GPIO_HOLD_KEY        21
#define S1_GPIO_HOME_KEY        35
#define S1_GPIO_SEND_KEY        83
#define S1_GPIO_MENU_KEY        94
#define S1_GPIO_VOLUME_DOWN_KEY 141
#define S1_GPIO_VOLUME_UP_KEY   145
#define S1_GPIO_END_KEY         152
#define S1_GPIO_CLEAR_KEY       153
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02)
#define S1_GPIO_HOLD_KEY        21
#define S1_GPIO_HOME_KEY        35
#define S1_GPIO_SEND_KEY        26
#define S1_GPIO_MENU_KEY        27
#define S1_GPIO_VOLUME_DOWN_KEY 141
#define S1_GPIO_VOLUME_UP_KEY   145
#define S1_GPIO_END_KEY         152
#define S1_GPIO_CLEAR_KEY       153
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#define S1_GPIO_HOME_KEY        35
#define S1_GPIO_VOLUME_DOWN_KEY 141
#define S1_GPIO_VOLUME_UP_KEY   145
#define S1_GPIO_SEARCH_KEY      152
#endif

/* Accessory */
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02)
#define S1_GPIO_ICC_DEVICE_nDET 83
#define S1_GPIO_ICC_REMOTE_nDET 94
#define S1_GPIO_JACK_SENSE      139
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#define S1_GPIO_ICC_DEVICE_nDET 83
#define S1_GPIO_EAR_KEY_nDET    94
#define S1_GPIO_JACK_SENSE      139
#endif

/* BT AUX_PCM */
#define S1_GPIO_AUX_PCM_DIN     69
#define S1_GPIO_AUX_PCM_DOUT    68
#define S1_GPIO_AUX_PCM_SYNC    70
#define S1_GPIO_AUX_PCM_CLK     71

/* Transfer Jet */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01) && (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_ES00)
#define S1_GPIO_TRJ_EN     84
#define S1_GPIO_TRJ_CLK    88
#define S1_GPIO_TRJ_CMD    89
#define S1_GPIO_TRJ_D3     90
#define S1_GPIO_TRJ_D2     91
#define S1_GPIO_TRJ_D1     92
#define S1_GPIO_TRJ_D0     93
#endif

/* Sensors */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#define S1_GPIO_ALPS_EN       22
#define S1_GPIO_COMPASS_nINT  27
#endif
#define S1_GPIO_3AXIS_nINT    42	/* 3AXIS sensor interrupt     */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_PT02)
#define S1_GPIO_COMPASS_nRST  30
#endif
#define S1_GPIO_PS_nINT       146	/* Proximity sensor interrupt */

/* Cameras */
#define S1_GPIO_CAM_CLK_EN       16

#define S1_GPIO_MAIN_CAM_EN      1
#define S1_GPIO_MAIN_CAM_STANDBY 2
#define S1_GPIO_MAIN_CAM_nRST    3

#define S1_GPIO_SUB_CAM_EN       0
#define S1_GPIO_SUB_CAM_STANDBY  142
#define S1_GPIO_SUB_CAM_nRST     143

/* LCD */
#define S1_GPIO_SPI_CLK      17
#define S1_GPIO_SPI_nCS      20
#define S1_GPIO_SPI_MOSI     18
#define S1_GPIO_SPI_MISO     19
#define S1_GPIO_LCD_BL       33
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
#define S1_GPIO_LCD_nRESET   133
#else
#define S1_GPIO_LCD_nRESET   106
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
#define S1_LCD_VREG_LEVEL    2800
#else
#define S1_LCD_VREG_LEVEL    2600
#endif

/* TDMB */
#define S1_GPIO_TDMB_nRESET      97	/* TDMB reset      */
#define S1_GPIO_TDMB_TSIF_DATA   107
#define S1_GPIO_TDMB_TSIF_EN     108
#define S1_GPIO_TDMB_TSIF_CLK    109

/* Touch PAD */
#define S1_GPIO_TOUCHPAD_nRESET  38
#define S1_GPIO_TOUCHPAD_nINT    39
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
#define S1_GPIO_5V_TOUCH_EN      26
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
#define S1_GPIO_TOUCH_KEY_BL_CTL 26
#endif

/* Charger */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)
#define S1_GPIO_CHARGER_nDET     21
#define S1_GPIO_TTA_nDET         32
#define S1_GPIO_CHARGER_STATUS   48
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES01)
#define S1_GPIO_CHARGER_EN       85
#endif

/* Fuel Gauge */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
#define S1_GPIO_BAT_nINT           105
#endif
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES03)
#define S1_GPIO_FUEL_GAUGE_EN      93
#endif

/* ETCs */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_PT02) && (CONFIG_S1_BOARD_VER <= S1_BOARD_VER_WS01)
#define S1_GPIO_ERM_MOTOR_EN       23
#endif
#define S1_GPIO_PM_nINT            24
#define S1_GPIO_PS_HOLD            25
#define S1_GPIO_AUDIO_AMP_nRESET   34	/* Audio AMP reset */
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
#define S1_GPIO_MIC_EN             15
#endif

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
#define S1_GPIO_UPPER_MOTOR_SLEEP  21
#define S1_GPIO_UPPER_MOTOR_EN     110
#define S1_GPIO_FM_EN              32
#endif

#define DECLARE_MSM_IOMAP
#include <mach/msm_iomap.h>


#endif /* GUARD */
