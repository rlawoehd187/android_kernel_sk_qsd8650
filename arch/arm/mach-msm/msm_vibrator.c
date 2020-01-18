/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/sched.h>

#include <mach/msm_rpcrouter.h>

#include <mach/board-s1.h>

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)

#define PM_LIBPROG      0x30000061
#ifdef CONFIG_MACH_QSD8X50_S1
#define PM_LIBVERS      0x10001
#else
#if (CONFIG_MSM_AMSS_VERSION == 6220) || (CONFIG_MSM_AMSS_VERSION == 6225)
#define PM_LIBVERS      0xfb837d0b
#else
#define PM_LIBVERS      0x10001
#endif
#endif

#ifdef CONFIG_MACH_QSD8X50_S1
#define ONCRPC_PM_VIB_MOT_SET_VOLT_PROC 22
#else
#define HTC_PROCEDURE_SET_VIB_ON_OFF	21
#endif
#define PMIC_VIBRATOR_LEVEL	(3000)

#ifdef CONFIG_MACH_QSD8X50_S1
static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;
#else
static struct work_struct work_vibrator_on;
static struct work_struct work_vibrator_off;
#endif

static void set_pmic_vibrator(int on)
{
	static struct msm_rpc_endpoint *vib_endpoint;
	struct set_vib_on_off_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	if (!vib_endpoint) {
		vib_endpoint = msm_rpc_connect(PM_LIBPROG, PM_LIBVERS, 0);
		if (IS_ERR(vib_endpoint)) {
			printk(KERN_ERR "init vib rpc failed!\n");
			vib_endpoint = 0;
			return;
		}
	}

	if (on)
		req.data = cpu_to_be32(PMIC_VIBRATOR_LEVEL);
	else
		req.data = cpu_to_be32(0);

#ifdef CONFIG_MACH_QSD8X50_S1
	msm_rpc_call(vib_endpoint, ONCRPC_PM_VIB_MOT_SET_VOLT_PROC, &req, sizeof(req), 5 * HZ);
#else
	msm_rpc_call(vib_endpoint, HTC_PROCEDURE_SET_VIB_ON_OFF, &req,
		sizeof(req), 5 * HZ);
#endif
}

#ifdef CONFIG_MACH_QSD8X50_S1
static void update_vibrator(struct work_struct *work)
{
	set_pmic_vibrator(vibe_state);
}
#else
static void pmic_vibrator_on(struct work_struct *work)
{
	set_pmic_vibrator(1);
}

static void pmic_vibrator_off(struct work_struct *work)
{
	set_pmic_vibrator(0);
}

static void timed_vibrator_on(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_on);
}

static void timed_vibrator_off(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_off);
}
#endif

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
#ifdef CONFIG_MACH_QSD8X50_S1		
	unsigned long	flags;

	spin_lock_irqsave(&vibe_lock, flags);
#endif
	hrtimer_cancel(&vibe_timer);

	if (value == 0)
#ifdef CONFIG_MACH_QSD8X50_S1		
		vibe_state = 0;
#else
		timed_vibrator_off(dev);
#endif		
	else {
		value = (value > 15000 ? 15000 : value);
#ifdef CONFIG_MACH_QSD8X50_S1		
		vibe_state = 1;
#else
		timed_vibrator_on(dev);
#endif

		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
#ifdef CONFIG_MACH_QSD8X50_S1	
	spin_unlock_irqrestore(&vibe_lock, flags);

	schedule_work(&vibrator_work);
#endif	
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		return r.tv.sec * 1000 + r.tv.nsec / 1000000;
	} else
		return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
#ifdef CONFIG_MACH_QSD8X50_S1
	vibe_state = 0;
	schedule_work(&vibrator_work);
#else	
	timed_vibrator_off(NULL);
#endif
	return HRTIMER_NORESTART;
}

static struct timed_output_dev pmic_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

void __init msm_init_pmic_vibrator(void)
{
#ifdef CONFIG_MACH_QSD8X50_S1
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	vibe_state = 0;
#else	
	INIT_WORK(&work_vibrator_on, pmic_vibrator_on);
	INIT_WORK(&work_vibrator_off, pmic_vibrator_off);
#endif

	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&pmic_vibrator);
}

MODULE_DESCRIPTION("timed output pmic vibrator device");
MODULE_LICENSE("GPL");

#endif
