#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/font.h>


#define BGR_BLUE	0xFF0000
#define BGR_WHITE	0xFFFFFF
#define BGR_BLACK	0x000000

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * ((fb)->var.bits_per_pixel / 8))

#ifdef CONFIG_PANIC_LOG_SAVE
extern int panic_skip;
#endif

struct pos {
	int x;
	int y;
};

#if 0
unsigned font5x12[] = {
    0x00000000, 0x00000000,
    0x08421080, 0x00020084,
    0x00052940, 0x00000000,
    0x15f52800, 0x0000295f,
    0x1c52f880, 0x00023e94,
    0x08855640, 0x0004d542,
    0x04528800, 0x000b2725,
    0x00021080, 0x00000000,
    0x04211088, 0x00821042,
    0x10841082, 0x00221108,
    0x09575480, 0x00000000,
    0x3e420000, 0x00000084,
    0x00000000, 0x00223000,
    0x3e000000, 0x00000000,
    0x00000000, 0x00471000,
    0x08844200, 0x00008442,
    0x2318a880, 0x00022a31,
    0x08429880, 0x000f9084,
    0x1108c5c0, 0x000f8444,
    0x1c4443e0, 0x00074610,
    0x14a62100, 0x000423e9,
    0x26d087e0, 0x00074610,
    0x1e10c5c0, 0x00074631,
    0x088443e0, 0x00010844,
    0x1d18c5c0, 0x00074631,
    0x3d18c5c0, 0x00074610,
    0x08e20000, 0x00471000,
    0x08e20000, 0x00223000,
    0x02222200, 0x00082082,
    0x01f00000, 0x000003e0,
    0x20820820, 0x00008888,
    0x1108c5c0, 0x00020084,
    0x2b98c5c0, 0x000f05b5,
    0x2318a880, 0x0008c63f,
    0x1d2949e0, 0x0007ca52,
    0x0210c5c0, 0x00074421,
    0x252949e0, 0x0007ca52,
    0x1e1087e0, 0x000f8421,
    0x1e1087e0, 0x00008421,
    0x0210c5c0, 0x00074639,
    0x3f18c620, 0x0008c631,
    0x084211c0, 0x00071084,
    0x10842380, 0x00032508,
    0x0654c620, 0x0008c525,
    0x02108420, 0x000f8421,
    0x2b5dc620, 0x0008c631,
    0x2b59ce20, 0x0008c739,
    0x2318c5c0, 0x00074631,
    0x1f18c5e0, 0x00008421,
    0x2318c5c0, 0x01075631,
    0x1f18c5e0, 0x0008c525,
    0x1c10c5c0, 0x00074610,
    0x084213e0, 0x00021084,
    0x2318c620, 0x00074631,
    0x1518c620, 0x0002114a,
    0x2b18c620, 0x000556b5,
    0x08a54620, 0x0008c54a,
    0x08a54620, 0x00021084,
    0x088443e0, 0x000f8442,
    0x0421084e, 0x00e10842,
    0x08210420, 0x00084108,
    0x1084210e, 0x00e42108,
    0x0008a880, 0x00000000,
    0x00000000, 0x01f00000,
    0x00000104, 0x00000000,
    0x20e00000, 0x000b663e,
    0x22f08420, 0x0007c631,
    0x22e00000, 0x00074421,
    0x23e84200, 0x000f4631,
    0x22e00000, 0x0007443f,
    0x1e214980, 0x00010842,
    0x22e00000, 0x1d187a31,
    0x26d08420, 0x0008c631,
    0x08601000, 0x00071084,
    0x10c02000, 0x0c94a108,
    0x0a908420, 0x0008a4a3,
    0x084210c0, 0x00071084,
    0x2ab00000, 0x0008d6b5,
    0x26d00000, 0x0008c631,
    0x22e00000, 0x00074631,
    0x22f00000, 0x0210be31,
    0x23e00000, 0x21087a31,
    0x26d00000, 0x00008421,
    0x22e00000, 0x00074506,
    0x04f10800, 0x00064842,
    0x23100000, 0x000b6631,
    0x23100000, 0x00022951,
    0x23100000, 0x000556b5,
    0x15100000, 0x0008a884,
    0x23100000, 0x1d185b31,
    0x11f00000, 0x000f8444,
    0x06421098, 0x01821084,
    0x08421080, 0x00021084,
    0x30421083, 0x00321084,
    0x0004d640, 0x00000000,
    0x00000000, 0x00000000,
};
#endif

static struct pos		cur_pos;
static struct pos		max_pos;
static struct font_desc		*hFont;

static void disp_drawglyph(uint32_t *pixels, uint32_t paint, unsigned stride,
			    unsigned char *glyph)
{
	unsigned x, y;
	unsigned char data;
	stride -= hFont->width;

	for (y = 0; y < (hFont->height); y++) 
	{
		for (x = 0; x < hFont->width; x++) 
		{
			if ( (x & 0x07) == 0 )
			{
				data = *glyph++;
			}
			
			if (data & 0x80)
			{
				*pixels = paint;
			}
			data <<= 1;
			pixels++;
		}
		pixels += stride;
	}
}

static void disp_scroll_up(void)
{
	struct fb_info *info;
	uint32_t *dst,*dst2;
	uint32_t *src;
	unsigned count;

	info = registered_fb[0];
	dst=(uint32_t *) info->screen_base;
	dst2 = dst+fb_size(info)/sizeof(uint32_t);
	src =dst + (fb_width(info) * hFont->height);
	count = fb_width(info) * (fb_height(info) - hFont->height);

	while(count--) {
		*dst++ = *src;
		*dst2++ = *src++;
	}
 
	count = fb_width(info) * hFont->height;
	while(count--) {
		*dst++ = BGR_BLACK ;
		*dst2++ = BGR_BLACK;
	}
}

void disp_clear(void)
{
	struct fb_info *info;
	uint32_t *dst, *dst2;
	unsigned count;

	info = registered_fb[0];
	dst =(uint32_t*) info->screen_base;
	dst2 = dst +fb_size(info)/sizeof(uint32_t);
	count = fb_width(info) * fb_height(info);

	cur_pos.x = 0;
	cur_pos.y = 0;
	while (count--) {
		*dst++ = BGR_BLACK;
		*dst2++ = BGR_BLACK;
	}
} 

void disp_log_putc(char c)
{
	uint32_t *pixels;
	struct fb_info *info;
	int    offset;
	unsigned char *pFontData;

	info = registered_fb[0];

	if (!info) {
		printk(KERN_WARNING "%s: disp_log_putc\n",__func__);
		return;
	}

	if((unsigned char)c > 127)
		return;
	if((unsigned char)c < 32) {
		if(c == '\n')
			goto newline;
		else if (c == '\r')
			cur_pos.x = 0;
		return;
	}

	pixels = (uint32_t*)info->screen_base;
	pixels += cur_pos.y * hFont->height * fb_width(info);
	pixels += cur_pos.x * hFont->width;
	offset  = c * ((hFont->width + 7) / 8) * hFont->height;
	pFontData = (unsigned char*)hFont->data;
	pFontData += offset;
	disp_drawglyph(pixels, BGR_WHITE, fb_width(info), pFontData);

	pixels = (uint32_t*)info->screen_base;
	pixels += (fb_size(info) / 4);
	pixels += cur_pos.y * hFont->height * fb_width(info);
	pixels += cur_pos.x * hFont->width;
	disp_drawglyph(pixels, BGR_WHITE, fb_width(info), pFontData);

	cur_pos.x++;
	if (cur_pos.x < max_pos.x)
		return;

newline:
	cur_pos.y++;
	cur_pos.x = 0;
	if(cur_pos.y >= max_pos.y) {
		cur_pos.y = max_pos.y - 1;
		disp_scroll_up();
	}
}

void disp_log_setup(void)
{
	struct fb_info *info;
	info = registered_fb[0];

	cur_pos.x = 0;
	cur_pos.y = 0;

	hFont = find_font("SUN12x22");
	max_pos.x = fb_width(info) / (hFont->width + 1);
	max_pos.y = (fb_height(info) -1) / (hFont->height);
	disp_clear();
}

int disp_log_dputs(const char *str)
{
	static int init=0;

	if (init == 0)
	{
		disp_log_setup();
		init =1;
	}
	
	while(*str != 0) {
		disp_log_putc(*str++);
	}
	return 0;
}
EXPORT_SYMBOL(disp_log_dputs);

asmlinkage int dispdebug(const char *fmt, ...)
{
	char buf[256];
	int err;
	va_list args;
	
	if (panic_skip)
		return 0;

	va_start(args, fmt);
	err = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	disp_log_dputs(buf);
	return err;
}
