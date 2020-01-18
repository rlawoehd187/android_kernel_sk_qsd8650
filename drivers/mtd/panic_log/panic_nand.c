/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include "panic_nand.h"

#define __LOG_BUF_LEN	(1 << (CONFIG_LOG_BUF_SHIFT )) //128K 
#define __LOG_ALLOC_BUF_LEN (__LOG_BUF_LEN>>1)

static struct mtd_info* panic_mtd;

extern int get_printk_log(char* buf, int len);
extern void logger_get_log_init(void);
extern int logger_get_logcat(char* buf);

static char panic_log_buf[__LOG_BUF_LEN];
static loff_t kernel_pos=0; // 3M이후..
int panic_skip =0;

int copy_printk_log(char* buf)
{
	int n;

	n = get_printk_log(buf, __LOG_ALLOC_BUF_LEN);
	return n;
}
EXPORT_SYMBOL(copy_printk_log);

int copy_logcat_log(char* buf)
{
	int i =0;
	
	logger_get_log_init();
	i= logger_get_logcat(buf);
	return i;
}	
EXPORT_SYMBOL(copy_logcat_log);

int panic_write_log(void)
{
	int n, ret;
	//struct mtd_oob_ops ops;
	size_t retlen;
	char partname[] = "dump";

	if (panic_skip)
		return 0;

	panic_skip =1;
	
	memset(panic_log_buf, 0x00, __LOG_BUF_LEN);

	n = copy_printk_log(panic_log_buf);
	n = copy_logcat_log(&panic_log_buf[__LOG_ALLOC_BUF_LEN]);
	
	panic_mtd =get_mtd_device_nm(partname);

	if (!panic_mtd)
	{
		return -1;
	}
#if 0
	if (panic_mtd->block_isbad(panic_mtd, kernel_pos) < 0) 
	{
		kernel_pos += panic_mtd->erasesize; //bad block여유 분이 1개라서 ...

		if (panic_mtd->size <= kernel_pos + panic_mtd->erasesize)
			return -1;
	}
	
	ops.mode = MTD_OOB_AUTO;
	ops.datbuf = panic_log_buf;
	ops.oobbuf = NULL;
	ops.ooblen = 0;
	ops.len = __LOG_BUF_LEN;

	ret =panic_mtd->write_oob(panic_mtd, kernel_pos, &ops);
#endif
	ret = panic_mtd->panic_write(panic_mtd, kernel_pos, __LOG_BUF_LEN, &retlen, panic_log_buf);
	return ret;
}
EXPORT_SYMBOL(panic_write_log);
