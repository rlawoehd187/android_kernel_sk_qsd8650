/* arch/arm/mach-msm/msm_gpio_vibrator.c
 *
 * Copyright (C) 2008 SK Telesys, Inc.
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
#include <mach/gpio.h>
#include <mach/board-s1.h>

#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02) || (CONFIG_S1_BOARD_VER == S1_BOARD_VER_WS01)
#define ON                1
#define OFF               0

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
#define MOTOR_UPPER
#undef MOTOR_ERM
#endif

static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;

static void set_gpio_vibrator(int on)
{
	if (on)
	{
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)

#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	#ifdef MOTOR_UPPER
		gpio_set_value(S1_GPIO_UPPER_MOTOR_EN, ON);
	#endif
	#ifdef MOTOR_ERM
		gpio_set_value(S1_GPIO_ERM_MOTOR_EN, ON);
	#endif
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_PT02)
		gpio_set_value(S1_GPIO_ERM_MOTOR_EN, ON);
#endif
	}
	else
	{   
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES00)

#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	#ifdef MOTOR_UPPER
		gpio_set_value(S1_GPIO_UPPER_MOTOR_EN, OFF);
	#endif
	#ifdef MOTOR_ERM
		gpio_set_value(S1_GPIO_ERM_MOTOR_EN, OFF);
	#endif
#elif (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_PT02)
		gpio_set_value(S1_GPIO_ERM_MOTOR_EN, OFF);
#endif
	}
}

static void update_vibrator(struct work_struct *work)
{
	set_gpio_vibrator(vibe_state);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	unsigned long	flags;

	spin_lock_irqsave(&vibe_lock, flags);
	hrtimer_cancel(&vibe_timer);

	if (value == 0)
		vibe_state = 0;
	else {
		value = (value > 15000 ? 15000 : value);
		vibe_state = 1;
		hrtimer_start(&vibe_timer,
			ktime_set(value / 1000, (value % 1000) * 1000000),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&vibe_lock, flags);

	schedule_work(&vibrator_work);
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
	vibe_state = 0;
	schedule_work(&vibrator_work);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev gpio_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

void __init msm_init_gpio_vibrator(void)
{
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&gpio_vibrator);
}

MODULE_DESCRIPTION("timed output gpio vibrator device");
MODULE_LICENSE("GPL");
#endif

