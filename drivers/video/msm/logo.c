/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>

#ifdef CONFIG_MACH_QSD8X50_S1
#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * ((fb)->var.bits_per_pixel / 8))


#if 0
static void memset32(void *_ptr, unsigned int val, unsigned count)
{
	unsigned int *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = val;
}

/* 888RLE image format: [count(4 bytes), rle(4 bytes)] */
int load_888rle_image(char *filename)
{
	struct fb_info *info;
	int fd, err = 0;
	unsigned count, max;
	unsigned int *data, *bits, *ptr;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s, fd=%d\n",
			__func__, filename, fd);
		return -ENOENT;
	}
	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	bits = (unsigned int *)(info->screen_base);
	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;
		memset32(bits, ptr[1], n << 1);
		bits += n;
		max -= n;
		ptr += 2;
		count -= 4;
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_888rle_image);
#endif

#ifdef CONFIG_FB_MSM_BOOTING_BAR
#define LOADING_BAR_COLOR       0x00000181  //color for progress bar : RED
#define LOADING_BAR_HIGHT       2 //pixels
#define LOADING_BAR_WIDTH       400 //pixels
#define LOADING_BAR_START_X     40
#define LOADING_BAR_END_X       LOADING_BAR_START_X + LOADING_BAR_WIDTH 
#define LOADING_BAR_START_Y     750
#define LOADING_BAR_END_Y       LOADING_BAR_START_Y + LOADING_BAR_HIGHT
#define MIN(x,y)                ( x < y ) ? x : y

extern char fBootingBar;

int load_booting_progress(int width)
{
	struct fb_info *info;
	static int start = LOADING_BAR_START_X;
	static int finish = LOADING_BAR_START_X;
	int x, y;
	unsigned int *fb_ptr;

	if(!fBootingBar)
		return 0;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",	__func__);
		return -ENODEV;
	}

	fb_ptr = (unsigned int *)(info->screen_base);

	if(width > 0)
		finish = MIN(finish + width , LOADING_BAR_END_X);

	for(x = start; x < finish; x++)
	{
		for(y = LOADING_BAR_START_Y; y < LOADING_BAR_END_Y; y++)
		{
			*(fb_ptr + x + (y*fb_width(info))) = LOADING_BAR_COLOR;
		}
	}

	//printk(KERN_INFO "%s] width:%d, total:%d\n", __func__, width, finish);
	start = finish;
	if(finish >= LOADING_BAR_END_X)
		fBootingBar = 0;

	return 0;
}
EXPORT_SYMBOL(load_booting_progress);
#endif
#else

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)

static void memset16(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = val;
}

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int fd, err = 0;
	unsigned count, max;
	unsigned short *data, *bits, *ptr;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	bits = (unsigned short *)(info->screen_base);
	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;
		memset16(bits, ptr[1], n << 1);
		bits += n;
		max -= n;
		ptr += 2;
		count -= 4;
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);

#endif
