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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/vreg.h>
#include <mach/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <mach/board-s1.h>
#include "tdmb_t3700.h"
#include "INC_INCLUDES.h"
#include <linux/vmalloc.h>

/* Each client has this additional data */
struct t3700_data 
{
	struct mutex update_lock;
	int	nOpenCtr;
};

static struct i2c_client *m_pDMBClient;
static u8     m_ucDevAddr = 0x80;


static int tdmb_power(int enable)
{
	struct vreg 	*vreg_tdmb;
	int		rc = 0;

	gpio_set_value(S1_GPIO_TDMB_nRESET, 0);
	vreg_tdmb = vreg_get(NULL, "gp5");
	if (IS_ERR(vreg_tdmb)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_tdmb));
		goto exit;
	}

	if (enable)
	{
		/* units of mV, steps of 50 mV */
		rc = vreg_set_level(vreg_tdmb, 1800);
		if (rc) {
			printk(KERN_ERR "%s: vreg tdmb set level failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
		rc = vreg_enable(vreg_tdmb);
		if (rc) {
			printk(KERN_ERR "%s: vreg tdmb enable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}

		mdelay(5);
		gpio_set_value(S1_GPIO_TDMB_nRESET, 1);
		mdelay(5);
	}
	else
	{
		rc = vreg_disable(vreg_tdmb);
		if (rc) {
			printk(KERN_ERR "%s: vreg tdmb disable failed (%d)\n",
			       __func__, rc);
			goto exit;
		}
	}
	printk(KERN_DEBUG "tdmb power switch: %s\n", enable ? "on" : "off");

exit:
	return (rc);
}


s32 t3700_write_word_data(u16 wAddr, u16 wData)
{
	s32	ret;
	u8	aucBuf[4];

	aucBuf[0] = (wAddr >> 8) & 0xff;
	aucBuf[1] = wAddr & 0xff;
	aucBuf[2] = (wData >> 8) & 0xff;
	aucBuf[3] = wData & 0xff;
	ret  = i2c_master_send(m_pDMBClient, (u8*)aucBuf, 4);

	return (ret);
}

s32 t3700_read_word_data(u16 wAddr, u16 *pwData)
{
	s32	ret;
	u8	aucBuf[2];

	aucBuf[0] = (wAddr >> 8) & 0xff;
	aucBuf[1] = wAddr & 0xff;
	ret  = i2c_master_send(m_pDMBClient, (u8*)aucBuf, 2);
	ret |= i2c_master_recv(m_pDMBClient, (u8*)aucBuf, 2);
	*pwData = (u16)(aucBuf[0] << 8) | aucBuf[1];

	return (ret);
}

s32 t3700_read_block_data(u16 wAddr, u8 *pData, u16 wSize)
{
	s32	ret;
	u8	aucBuf[2];

	aucBuf[0] = (wAddr >> 8) & 0xff;
	aucBuf[1] = wAddr & 0xff;	
	ret  = i2c_master_send(m_pDMBClient, aucBuf, 2);
	ret |= i2c_master_recv(m_pDMBClient, pData, wSize);

	return (ret);
}


static int t3700_open(struct inode *inode, struct file *filp)
{
	struct t3700_data *pData;
	int	rc = 0;
	
	pData = i2c_get_clientdata(m_pDMBClient);
	if (pData->nOpenCtr > 0)
	{
		rc = -EBUSY;
		goto exit;
	}

	if (tdmb_power(1) != 0)
	{
		rc = -EIO;
		goto exit;
	}

	pData->nOpenCtr++;
	INTERFACE_INIT(m_ucDevAddr);
exit:	
	return rc;
}

static int t3700_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	int i=0;
	
	if (_IOC_TYPE(cmd) != T3700_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) >= T3700_IOCTL_MAX)
		return -EINVAL;

	switch(cmd)
	{
		case T3700_GET_FREQ_CNT:
			{
				signed short nCtr;

				nCtr = MAX_KOREABAND_FULL_CHANNEL;
				put_user(nCtr, (signed short __user *)arg);
			}
			break;

		case T3700_SCAN_FREQ_INDEX:
			{
				signed short 	nIndex;
				ST_FIC_DB 	*pDB;
				CH_INFO_T 	*pDst = (CH_INFO_T*)arg;
				CH_INFO_T	*pChInfo;

				if (copy_from_user(&nIndex, &pDst->nIndex, sizeof(nIndex)))
				{
					rc = -EFAULT;
					goto exit;

				}
				if (INTERFACE_SCAN(m_ucDevAddr, INC_GET_KOREABAND_FULL_TABLE(nIndex)) != INC_ERROR)
				{
					pDB = INC_GETFIC_DB(m_ucDevAddr);

					pChInfo = kmalloc(sizeof(CH_INFO_T), GFP_KERNEL);
					if (pChInfo == NULL)
					{
						rc = -ENOBUFS;
						goto exit;
					}
					pChInfo->nIndex     = nIndex;
					pChInfo->ulRFFreq   = pDB->ulRFFreq;
					pChInfo->ucSubChCnt = pDB->ucSubChCnt;
					memcpy(&pChInfo->aucSubChID, pDB->aucSubChID, sizeof(pDst->aucSubChID));
					memcpy(&pChInfo->aucDSCType, pDB->aucDSCType, sizeof(pDst->aucDSCType));
					memcpy(&pChInfo->aucEnsembleLabel, pDB->aucEnsembleLabel, sizeof(pDst->aucEnsembleLabel));
					memcpy(&pChInfo->aucServiceLabel,  pDB->aucServiceLabel,  sizeof(pDst->aucServiceLabel));
					memset(&pChInfo->aucDMBType, 0x00, sizeof(pChInfo->aucDMBType));
					
					for ( i=0; i< pDB->ucSubChCnt; i++)
					{
				
						if (pDB->aucDSCType[i] == 0x18) 
						{
							if (pDB->astUserAppInfo.uiUserAppType[i] == 0x09 && \
								pDB->astUserAppInfo.ucUserAppDataLength[i] ==2 && pDB->astUserAppInfo.aucUserAppData[i][1] == 0x02)
							{
								pChInfo->aucDMBType[i] =0x1;
							}
							else if (pDB->astSchDb[i].uiBitRate <= 192) // Visual Radio info 가 정확하지 않음..
							{
								pChInfo->aucDMBType[i] =0x1;
							}
						}
					}
					
					if (copy_to_user(pDst, pChInfo, sizeof(CH_INFO_T)))
					{
						rc = -EFAULT;
						kfree(pChInfo);
						goto exit;
					}
					else
					{
						kfree(pChInfo);
					}
				}
				else
				{
					rc = -ENOENT;
				}
			}
			break;
			
		
		case T3700_SET_CH:
			{
				CH_SET_INFO_T	sSetChInfo;
				CH_SET_INFO_T	*pSrc = (CH_SET_INFO_T*)arg;
				INC_CHANNEL_INFO *pIncChInfo;
				
				if (copy_from_user(&sSetChInfo, pSrc, sizeof(sSetChInfo)))
				{
					rc = -EFAULT;
					goto exit;
				}

				pIncChInfo = kmalloc(sizeof(INC_CHANNEL_INFO), GFP_KERNEL);
				if (pIncChInfo == NULL)
				{
					rc = -ENOBUFS;
					goto exit;
				}
				pIncChInfo->ucServiceType = sSetChInfo.ucDSCType;
				pIncChInfo->ucSubChID     = sSetChInfo.ucSubChID;
				pIncChInfo->ulRFFreq      = sSetChInfo.ulRFFreq;
				
				sSetChInfo.ucStatus=INTERFACE_START(m_ucDevAddr, pIncChInfo);
				kfree(pIncChInfo);
				
				if (copy_to_user((CH_SET_INFO_T*)arg, &sSetChInfo, sizeof(sSetChInfo)))
				{
					rc = -EFAULT;
					goto exit;
				}
			}
			break;

		case T3700_GET_CER:
			{
				CH_GET_RF_INFO_T sRFInfo;

				sRFInfo.uiCER     = INTERFACE_GET_CER(m_ucDevAddr);
				sRFInfo.ulPostBER = INTERFACE_GET_POSTBER(m_ucDevAddr);
				sRFInfo.ulPreBER  = INTERFACE_GET_PREBER(m_ucDevAddr);
				
				if (copy_to_user((CH_GET_RF_INFO_T*)arg, &sRFInfo, sizeof(sRFInfo)))
				{
					rc = -EFAULT;
					goto exit;
				}
			}
			break;

		case T3700_STOP_CH:
			{
				INTERFACE_USER_STOP(m_ucDevAddr);
				INC_STOP(m_ucDevAddr);
			}
			break;

		case T3700_GET_SCAN_LIST:
			{
				CH_FULL_LIST_T* pChListInfo;
				INC_CHANNEL_INFO* info;
				int i;
				
				pChListInfo = vmalloc(sizeof(CH_FULL_LIST_T));

				pChListInfo->scanInfo.iDmbCnt = INTERFACE_GETDMB_CNT();
				pChListInfo->scanInfo.iDabCnt = INTERFACE_GETDAB_CNT();
				pChListInfo->scanInfo.iDataCnt = INTERFACE_GETDATA_CNT();

				if ((pChListInfo->scanInfo.iDmbCnt > MAX_SUBCHANNEL) ||\
					(pChListInfo->scanInfo.iDabCnt > MAX_SUBCHANNEL) ||\
					(pChListInfo->scanInfo.iDataCnt  > MAX_SUBCHANNEL))
				{
					vfree(pChListInfo);
					rc = -EFAULT;
					goto exit;
				}

				for (i=0; i< pChListInfo->scanInfo.iDmbCnt; i++)
				{
					info =  INTERFACE_GETDB_DMB(i);
					pChListInfo->dmbChannel[i].ulRFFreq = info->ulRFFreq;
					pChListInfo->dmbChannel[i].uiEnsembleID= info->uiEnsembleID;
					pChListInfo->dmbChannel[i].uiBitRate = info->uiBitRate;
					pChListInfo->dmbChannel[i].ucSubChID= info->ucSubChID;
					pChListInfo->dmbChannel[i].ucServiceID = info->ucServiceType;
					memcpy(pChListInfo->dmbChannel[i].aucLable, info->aucLabel, sizeof(info->aucLabel));
					memcpy(pChListInfo->dmbChannel[i].aucEnsembleLabel, info->aucEnsembleLabel, sizeof(info->aucEnsembleLabel));
				}

				for (i=0; i< pChListInfo->scanInfo.iDabCnt; i++)
				{
					info =  INTERFACE_GETDB_DAB(i);
					pChListInfo->dabChannel[i].ulRFFreq = info->ulRFFreq;
					pChListInfo->dabChannel[i].uiEnsembleID= info->uiEnsembleID;
					pChListInfo->dabChannel[i].uiBitRate = info->uiBitRate;
					pChListInfo->dabChannel[i].ucSubChID= info->ucSubChID;
					pChListInfo->dabChannel[i].ucServiceID = info->ucServiceType;
					memcpy(pChListInfo->dabChannel[i].aucLable, info->aucLabel, sizeof(info->aucLabel));
					memcpy(pChListInfo->dmbChannel[i].aucEnsembleLabel, info->aucEnsembleLabel, sizeof(info->aucEnsembleLabel));
				}

				for (i=0; i< pChListInfo->scanInfo.iDataCnt; i++)
				{
					info =  INTERFACE_GETDB_DATA(i);
					pChListInfo->dataChannel[i].ulRFFreq = info->ulRFFreq;
					pChListInfo->dataChannel[i].uiEnsembleID= info->uiEnsembleID;
					pChListInfo->dataChannel[i].uiBitRate = info->uiBitRate;
					pChListInfo->dataChannel[i].ucSubChID= info->ucSubChID;
					pChListInfo->dataChannel[i].ucServiceID = info->ucServiceType;
					memcpy(pChListInfo->dabChannel[i].aucLable, info->aucLabel, sizeof(info->aucLabel));
					memcpy(pChListInfo->dmbChannel[i].aucEnsembleLabel, info->aucEnsembleLabel, sizeof(info->aucEnsembleLabel));
				}
				if (copy_to_user((CH_FULL_LIST_T*)arg, pChListInfo, sizeof(CH_FULL_LIST_T)))
				{
					vfree(pChListInfo);
					rc = -EFAULT;
					goto exit;
				}
				vfree(pChListInfo);
			}
			break;
		default:
			break;
	}

exit:	
	return rc;
}

static int t3700_release(struct inode *inode, struct file *filp)
{
	struct t3700_data *pData;
	int	rc = 0;
	
	pData = i2c_get_clientdata(m_pDMBClient);
	if (pData->nOpenCtr != 1)
	{
		rc = -ENOENT;
		goto exit;
	}
	if (tdmb_power(0) != 0)
	{
		rc = -EIO;
		goto exit;
	}
	pData->nOpenCtr	= 0;

exit:	
	return rc;
}

static const struct file_operations t3700_fops = {
	.owner   = THIS_MODULE,
	.open    = t3700_open,
	.ioctl   = t3700_ioctl,
	.release = t3700_release,
};

static struct miscdevice t3700_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tdmb",
	.fops = &t3700_fops,
};


static int t3700_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct t3700_data *data;
	int 	rc;
	u16 	wID;
	u32	cfgDMB = GPIO_CFG(S1_GPIO_TDMB_nRESET, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
	u32     cfgFM  = GPIO_CFG(S1_GPIO_FM_EN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_tlmm_config(cfgFM, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
		       " failed: %d\n", GPIO_PIN(cfgDMB), rc);
		goto exit;
	}
	gpio_set_value(S1_GPIO_FM_EN, 0);
#endif

	rc = gpio_tlmm_config(cfgDMB, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
		       " failed: %d\n", GPIO_PIN(cfgDMB), rc);
		goto exit;
	}
	

	if (tdmb_power(1) != 0)
	{
		rc = -EIO;
		goto exit;
	}
	
	if (!(data = kzalloc(sizeof(struct t3700_data), GFP_KERNEL))) {
		rc = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	m_pDMBClient = client;
	rc = t3700_read_word_data(0x510, &wID);
	if (rc < 0)
		goto free_exit;

	rc = misc_register(&t3700_misc);
	if (unlikely(rc)) {
		printk(KERN_ERR "t3700: failed to register misc device!\n");
		goto free_exit;
	}

	if (tdmb_power(0) != 0)
	{
		rc = -EIO;
		goto exit;
	}

	return 0;

free_exit:
	kfree(data);
exit:
	return rc;
}


static int t3700_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));

	return 0;
}


static const struct i2c_device_id t3700_id[] = {
	{ "tdmb", 0 },
	{ }
};

#ifdef CONFIG_PM
static int t3700_pm_suspend(struct device *dev)
{
	struct t3700_data *pData;
	int rc = 0;
	
	pData = i2c_get_clientdata(m_pDMBClient);

	if (pData->nOpenCtr != 1)
	{
		rc =0;
		goto exit;
	}

	if (tdmb_power(0) != 0)
	{
		printk(KERN_ERR "%s fail to turn off power\n", __func__);
		goto exit;
	}

	printk(KERN_ERR "%s\n", __func__);
exit:
	return rc;
}

static int t3700_pm_resume(struct device *dev)
{
	struct t3700_data *pData;
	int rc = 0;
	
	pData = i2c_get_clientdata(m_pDMBClient);

	if (pData->nOpenCtr != 1)
	{
		rc =0;
		goto exit;
	}

	if (tdmb_power(1) != 0)
	{
		printk(KERN_ERR "%s fail to turn on power\n", __func__);
		goto exit;
	}

	INTERFACE_INIT(m_ucDevAddr);

	printk(KERN_ERR "%s\n", __func__);
exit:
	return rc;
}

static struct dev_pm_ops t3700_pm_ops = {
	.suspend = t3700_pm_suspend,
	.resume  = t3700_pm_resume,
};
#endif



static struct i2c_driver t3700_driver = {
	.driver = {
		.name	= "tdmb",
#if CONFIG_PM			
		.pm	    = &t3700_pm_ops,
#endif		
	},
	.probe		= t3700_probe,
	.remove		= t3700_remove,
	.id_table	= t3700_id,
	.class		= I2C_CLASS_HWMON,
};

static int __init t3700_init(void)
{
	int ret;

	ret = i2c_add_driver(&t3700_driver);
	printk(KERN_WARNING
		"t3700_init : %d\n", ret);

	return ret;
}

static void __exit t3700_exit(void)
{
	i2c_del_driver(&t3700_driver);
}


module_init(t3700_init);
module_exit(t3700_exit);

MODULE_AUTHOR("TJ Kim");
MODULE_DESCRIPTION("TDMB T3700 driver");
MODULE_LICENSE("GPL");
