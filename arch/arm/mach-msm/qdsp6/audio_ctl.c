/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/msm_audio.h>

#include <mach/msm_qdsp6_audio.h>
#include <mach/debug_mm.h>

#define BUFSZ (0)

static DEFINE_MUTEX(voice_lock);
static int voice_started;

static struct audio_client *voc_tx_clnt;
static struct audio_client *voc_rx_clnt;

static int q6_voice_start(void)
{
	int rc = 0;

	mutex_lock(&voice_lock);

	if (voice_started) {
		pr_err("[%s:%s] busy\n", __MM_FILE__, __func__);
		rc = -EBUSY;
		goto done;
	}

	voc_tx_clnt = q6voice_open(AUDIO_FLAG_WRITE);
	if (!voc_tx_clnt) {
		pr_err("[%s:%s] open voice tx failed.\n", __MM_FILE__,
				__func__);
		rc = -ENOMEM;
		goto done;
	}

	voc_rx_clnt = q6voice_open(AUDIO_FLAG_READ);
	if (!voc_rx_clnt) {
		pr_err("[%s:%s] open voice rx failed.\n", __MM_FILE__,
				__func__);
		q6voice_close(voc_tx_clnt);
		rc = -ENOMEM;
	}

	voice_started = 1;
done:
	mutex_unlock(&voice_lock);
	return rc;
}

static int q6_voice_stop(void)
{
	mutex_lock(&voice_lock);
	if (voice_started) {
		q6voice_close(voc_tx_clnt);
		q6voice_close(voc_rx_clnt);
		voice_started = 0;
	}
	mutex_unlock(&voice_lock);
	return 0;
}

static int q6_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int q6_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	int rc;
	uint32_t n;
	uint32_t id[2];

	switch (cmd) {
	case AUDIO_SWITCH_DEVICE:
		rc = copy_from_user(&id, (void *)arg, sizeof(id));
		if (!rc)
			rc = q6audio_do_routing(id[0], id[1]);
		break;
	case AUDIO_SET_VOLUME:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_set_rx_volume(n);
		break;
	case AUDIO_SET_MUTE:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_set_tx_mute(n);
		break;
	case AUDIO_UPDATE_ACDB:
		rc = copy_from_user(&id, (void *)arg, sizeof(id));
		if (!rc)
			rc = q6audio_update_acdb(id[0], 0);
		break;
	case AUDIO_START_VOICE:
		rc = q6_voice_start();
		break;
	case AUDIO_STOP_VOICE:
		rc = q6_voice_stop();
		break;
	case AUDIO_REINIT_ACDB:
		rc = 0;
		break;

#ifdef CONFIG_MACH_QSD8X50_S1
	case AUDIO_GET_TX_MUTE:
	{
		extern int q6audio_get_tx_mute(void);

		int mute = q6audio_get_tx_mute();

		rc = copy_to_user((void*)arg, &mute, sizeof(mute));
		if (!rc)
			rc = 0;
	}
	break;

	case AUDIO_SET_RX_MUTE:
	{
		extern int q6audio_set_rx_mute(int mute);
		
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_set_rx_mute(n);
	}
	break;

	case AUDIO_GET_RX_MUTE:
	{
		extern int mVoiceMute;

		rc = copy_to_user((void*)arg, &mVoiceMute, sizeof(mVoiceMute));
		if (!rc)
			rc = 0;
	}
	break;

	case AUDIO_GET_ACDB_INFO:
	{
		extern int acdb_get_config_table_ext(struct s1_tune_acdb_info *pInfo);
		struct s1_tune_acdb_info *pInfo;
		
		pInfo = kmalloc(sizeof(struct s1_tune_acdb_info), GFP_KERNEL);
		if (!pInfo)
		{
			rc = -EIO;
			break;
		}

		rc = -EFAULT;
		if (!copy_from_user(pInfo, (void *)arg, 3 * sizeof(uint32_t)))
		{
			rc = acdb_get_config_table_ext(pInfo);
			if (rc >= 0)
			{
				if (!copy_to_user((void*)arg, pInfo, sizeof(struct s1_tune_acdb_info)))
				{
					rc = 0;
				}
			}
		}
		kfree(pInfo);
	}
	break;

	case AUDIO_SET_ACDB_INFO:
	{
		extern int acdb_set_config_table_ext(struct s1_tune_acdb_info *pInfo);
		struct s1_tune_acdb_info *pInfo;
		
		pInfo = kmalloc(sizeof(struct s1_tune_acdb_info), GFP_KERNEL);
		if (!pInfo)
		{
			rc = -EIO;
			break;
		}

		rc = -EFAULT;
		if (!copy_from_user(pInfo, (void *)arg, sizeof(struct s1_tune_acdb_info)))
		{
			rc = acdb_set_config_table_ext(pInfo);
			if (rc == 0)
			{
				rc = 0;
			}
		}
		kfree(pInfo);
	}
	break;
#endif

	default:
		rc = -EINVAL;
	}

	return rc;
}


static int q6_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations q6_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_open,
	.ioctl		= q6_ioctl,
	.release	= q6_release,
};

struct miscdevice q6_control_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_ctl",
	.fops	= &q6_dev_fops,
};


static int __init q6_audio_ctl_init(void) {
	return misc_register(&q6_control_device);
}

device_initcall(q6_audio_ctl_init);
