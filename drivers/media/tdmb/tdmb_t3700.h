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

#ifndef _TDMB_T3700_H
#define _TDMB_T3700_H

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/i2c-id.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>	/* for struct device */
#include <linux/sched.h>	/* for completion */
#include <linux/mutex.h>


#define MAX_LABEL_CHAR										16
#define MAX_SUBCHANNEL										64

typedef struct
{
	signed	 short	nIndex;
	unsigned long	ulRFFreq;
	unsigned short  uiEnsembleID;
	unsigned char	ucSubChCnt;
	unsigned char	aucSubChID[MAX_SUBCHANNEL];
	unsigned char	aucDSCType[MAX_SUBCHANNEL];
	unsigned char	aucEnsembleLabel[MAX_LABEL_CHAR];
	unsigned char	aucServiceLabel[MAX_SUBCHANNEL][MAX_LABEL_CHAR];
	unsigned char aucDMBType[MAX_SUBCHANNEL];
} CH_INFO_T;

typedef struct
{
	unsigned long	ulRFFreq;
	unsigned char   ucSubChID;
	unsigned char	ucDSCType;
	unsigned char	ucStatus;
} CH_SET_INFO_T;

typedef struct
{
	unsigned long	ulPreBER;
	unsigned long   ulPostBER;
	unsigned short	uiCER;
} CH_GET_RF_INFO_T;

typedef struct
{
	int iDmbCnt;
	int iDabCnt;
	int iDataCnt;
} CH_SCAN_CNT_T;

typedef struct
{
	unsigned long  ulRFFreq;
	unsigned short uiEnsembleID;
	unsigned short uiBitRate;
	char                aucLable[MAX_LABEL_CHAR];
	unsigned char  ucSubChID;
	unsigned char  ucServiceID;
	char                aucEnsembleLabel[MAX_LABEL_CHAR];
}CH_SCAN_LIST_T;

typedef struct
{
	CH_SCAN_CNT_T scanInfo;
	CH_SCAN_LIST_T dmbChannel[MAX_SUBCHANNEL];
	CH_SCAN_LIST_T dabChannel[MAX_SUBCHANNEL];
	CH_SCAN_LIST_T dataChannel[MAX_SUBCHANNEL];
} CH_FULL_LIST_T;


#define	T3700_MAGIC	'T'
#define	T3700_IOCTL_MAX	6

#define T3700_GET_FREQ_CNT	_IOR (T3700_MAGIC, 0, signed short)
#define T3700_SCAN_FREQ_INDEX	_IOWR(T3700_MAGIC, 1, CH_INFO_T)
#define T3700_SET_CH		_IOW (T3700_MAGIC, 2, CH_SET_INFO_T)
#define T3700_GET_CER		_IOR (T3700_MAGIC, 3, CH_GET_RF_INFO_T)
#define T3700_STOP_CH		_IOW(T3700_MAGIC, 4, char)
#define T3700_GET_SCAN_LIST	_IOR (T3700_MAGIC, 5, CH_FULL_LIST_T)

extern s32 t3700_write_word_data(u16 wAddr, u16 wData);
extern s32 t3700_read_word_data(u16 wAddr, u16 *pwData);
extern s32 t3700_read_block_data(u16 wAddr, u8 *pData, u16 wSize);

#endif

#endif
