/*
 * 32bit compatibility wrappers for the input subsystem.
 *
 * Very heavily based on evdev.c - Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <asm/uaccess.h>
#include "input-compat.h"

//#define SKTS_LOG_SAVE

#ifdef SKTS_LOG_SAVE
#include <linux/rtc.h>
#include <linux/vfs.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#endif /*SKTS_LOG_SAVE*/

#ifdef CONFIG_COMPAT

int input_event_from_user(const char __user *buffer,
			  struct input_event *event)
{
	if (INPUT_COMPAT_TEST) {
		struct input_event_compat compat_event;

		if (copy_from_user(&compat_event, buffer,
				   sizeof(struct input_event_compat)))
			return -EFAULT;

		event->time.tv_sec = compat_event.time.tv_sec;
		event->time.tv_usec = compat_event.time.tv_usec;
		event->type = compat_event.type;
		event->code = compat_event.code;
		event->value = compat_event.value;

	} else {
		if (copy_from_user(event, buffer, sizeof(struct input_event)))
			return -EFAULT;
	}

	return 0;
}

int input_event_to_user(char __user *buffer,
			const struct input_event *event)
{
	if (INPUT_COMPAT_TEST) {
		struct input_event_compat compat_event;

		compat_event.time.tv_sec = event->time.tv_sec;
		compat_event.time.tv_usec = event->time.tv_usec;
		compat_event.type = event->type;
		compat_event.code = event->code;
		compat_event.value = event->value;

		if (copy_to_user(buffer, &compat_event,
				 sizeof(struct input_event_compat)))
			return -EFAULT;

	} else {
		if (copy_to_user(buffer, event, sizeof(struct input_event)))
			return -EFAULT;
	}

	return 0;
}

int input_ff_effect_from_user(const char __user *buffer, size_t size,
			      struct ff_effect *effect)
{
	if (INPUT_COMPAT_TEST) {
		struct ff_effect_compat *compat_effect;

		if (size != sizeof(struct ff_effect_compat))
			return -EINVAL;

		/*
		 * It so happens that the pointer which needs to be changed
		 * is the last field in the structure, so we can retrieve the
		 * whole thing and replace just the pointer.
		 */
		compat_effect = (struct ff_effect_compat *)effect;

		if (copy_from_user(compat_effect, buffer,
				   sizeof(struct ff_effect_compat)))
			return -EFAULT;

		if (compat_effect->type == FF_PERIODIC &&
		    compat_effect->u.periodic.waveform == FF_CUSTOM)
			effect->u.periodic.custom_data =
				compat_ptr(compat_effect->u.periodic.custom_data);
	} else {
		if (size != sizeof(struct ff_effect))
			return -EINVAL;

		if (copy_from_user(effect, buffer, sizeof(struct ff_effect)))
			return -EFAULT;
	}

	return 0;
}

#else

int input_event_from_user(const char __user *buffer,
			 struct input_event *event)
{
	if (copy_from_user(event, buffer, sizeof(struct input_event)))
		return -EFAULT;

	return 0;
}

#ifdef SKTS_LOG_SAVE
#define __LOG_BUF_LEN	(1 << CONFIG_LOG_BUF_SHIFT)
#define __LOG_ALLOC_BUF_LEN (__LOG_BUF_LEN>>1)

extern int copy_printk_log(char* buf);
extern int copy_logcat_log(char* buf);

int force_log_save(void)
{
	int n;
	char* buf=NULL;
	struct timeval tv;
	struct rtc_time tm;
	
	static struct file *file_p = NULL;
	char filename[45];

	int old_fs = get_fs();
	
	buf =vmalloc(__LOG_BUF_LEN);
	memset(buf, 0x00, __LOG_BUF_LEN);
	
	n= copy_printk_log(buf);
	n= copy_logcat_log(&buf[__LOG_ALLOC_BUF_LEN]);

	do_gettimeofday(&tv);
	tv.tv_sec -= (sys_tz.tz_minuteswest*60);
	rtc_time_to_tm(tv.tv_sec, &tm);

	memset(filename, 0x00, 45);

	sprintf(filename,"/data/sk-w/force_log_%02d-%02d[%02d.%02d.%02d].txt",
		tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	
	set_fs(KERNEL_DS);

	file_p = filp_open((const char*)filename, O_WRONLY | O_CREAT |O_APPEND, 0666);

	if (IS_ERR(file_p))
	{
		printk(KERN_ERR "force_log_save: open error\n");
		set_fs(old_fs);
		return -1;
	}
	

	vfs_write(file_p, (char __user*)buf, __LOG_BUF_LEN, &file_p->f_pos);
	filp_close(file_p, NULL);

	set_fs(old_fs);
	vfree(buf);
	return 0;
}

#endif /*SKTS_LOG_SAVE*/

int input_event_to_user(char __user *buffer,
			const struct input_event *event)
{
#ifdef SKTS_LOG_SAVE
	static int key_f23_push=0;
	static int key_f24_push=0;
	static int key_volup_push=0;
	static int key_voldown_push=0;
#endif

	if (copy_to_user(buffer, event, sizeof(struct input_event)))
		return -EFAULT;

#ifdef SKTS_LOG_SAVE
	if (event->type == EV_KEY){
		if (event->code == KEY_F23) {
			if (event->value == 1) {
				key_f23_push =1;
			}else {
				key_f23_push =0;
			}
		}
		else if (event->code == KEY_F24) {
			if (event->value == 1) {
				key_f24_push =1;
			}else{
				key_f24_push =0;
			}
		}
		else if (event->code == KEY_VOLUMEDOWN) {
			if (event->value == 1) {
				key_voldown_push =1;
			}else {
				key_voldown_push =0;
			}
		}
		else if (event->code == KEY_VOLUMEUP) {
			if (event->value == 1) {
				key_volup_push =1;
			}else {
				key_volup_push =0;
			}
		}

		if ((key_f23_push) && (key_f24_push) && (key_voldown_push) && (key_volup_push)) {
			force_log_save();
		}
	}
#endif
	return 0;
}

int input_ff_effect_from_user(const char __user *buffer, size_t size,
			      struct ff_effect *effect)
{
	if (size != sizeof(struct ff_effect))
		return -EINVAL;

	if (copy_from_user(effect, buffer, sizeof(struct ff_effect)))
		return -EFAULT;

	return 0;
}

#endif /* CONFIG_COMPAT */

EXPORT_SYMBOL_GPL(input_event_from_user);
EXPORT_SYMBOL_GPL(input_event_to_user);
EXPORT_SYMBOL_GPL(input_ff_effect_from_user);
