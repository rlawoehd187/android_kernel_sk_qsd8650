#ifndef __LINUX_I2C_TMA340_H
#define __LINUX_I2C_TMA340_H

/* linux/i2c/tma340.h */

struct tma340_platform_data {
	int gpioreset;
	int gpioirq;
	//u16	model;
	//u16	x_plate_ohms;

	//int	(*get_pendown_state)(void);
	//void	(*clear_penirq)(void);		/* If needed, clear 2nd level
	//					   interrupt source */
	int	(*init_platform_hw)(void);
	void	(*exit_platform_hw)(void);
};

struct cyttsp_gen3_xydata_t {
#if 0
	u8 hst_mode;
	u8 tt_mode;
#endif
	u8 tt_stat;
	u16 x1 __attribute__ ((packed));
	u16 y1 __attribute__ ((packed));
	u8 z1;
	u8 touch12_id;
	u16 x2 __attribute__ ((packed));
	u16 y2 __attribute__ ((packed));
	u8 z2;
#if 0
	u8 gest_cnt;
	u8 gest_id;
	u16 x3 __attribute__ ((packed));
	u16 y3 __attribute__ ((packed));
	u8 z3;
	u8 touch34_id;
	u16 x4 __attribute__ ((packed));
	u16 y4 __attribute__ ((packed));
	u8 z4;
#endif
	u8 int_flags;
};

struct cyttsp_xy_coordinate_t {
	u16 x __attribute__ ((packed));
	u16 y __attribute__ ((packed));
	u8  z;
};


#define TMA340_HST_MODE          0x00
#define TMA340_TRUETOUCH_STAT    0x02
#define TMA340_TOUCH1_XH         0x03
#define TMA340_TOUCH1_XL         0x04
#define TMA340_TOUCH1_YH         0x05
#define TMA340_TOUCH1_YL         0x06
#define TMA340_TOUCH1_Z          0x07
#define TMA340_TOUCH2_XH         0x09
#define TMA340_TOUCH2_XL         0x0A
#define TMA340_TOUCH2_YH         0x0B
#define TMA340_TOUCH2_YL         0x0C
#define TMA340_TOUCH_BTN_MASK    0x1B
#define TMA340_VERSION_INFO      0x1C
#define TMA340_EXTRA_FUNCTION    0x1D

#define TMA340_REG_BASE          TMA340_TRUETOUCH_STAT //TMA340_HST_MODE
#define TMA340_1STTOUCH_BASE     TMA340_TOUCH1_XH
#define TMA340_2NDTOUCH_BASE     TMA340_TOUCH2_XH

#define TMA340_MODE_LOW_POWER    0x04
#define TMA340_MODE_DEEP_SLEEP   0x02
#define TMA340_MODE_SOFT_RESET   0x01

//IOCTL
#define ISSP_IO_MAGIC                0xCE
#define ISSP_IOCTL_SCLK_TO_GPIO      _IOW(ISSP_IO_MAGIC, 0x0, short)
#define ISSP_IOCTL_DATA_TO_GPIO      _IOW(ISSP_IO_MAGIC, 0x1, short)
#define ISSP_IOCTL_SCLK              _IOW(ISSP_IO_MAGIC, 0x2, short)
#define ISSP_IOCTL_DATA              _IOW(ISSP_IO_MAGIC, 0x3, short)
#define ISSP_IOCTL_POWER             _IOW(ISSP_IO_MAGIC, 0x4, short)
#define ISSP_IOCTL_READ_DATA_PIN     _IOWR(ISSP_IO_MAGIC, 0x5, short)
#define ISSP_IOCTL_WAIT              _IOW(ISSP_IO_MAGIC, 0x6, unsigned int)
#define ISSP_IOCTL_RESET             _IOW(ISSP_IO_MAGIC, 0x7, short)
#define ISSP_IOCTL_INTR              _IOW(ISSP_IO_MAGIC, 0x8, short)
#define ISSP_IOCTL_READ_FW           _IOR(ISSP_IO_MAGIC, 0x9, int)
#define ISSP_IOCTL_CHANGE_PIN_MODE   _IOW(ISSP_IO_MAGIC, 0xA, short)
#define ISSP_IOCTL_EXTRA_FUNCTION    _IOW(ISSP_IO_MAGIC, 0xB, short)

#define CY_NUM_RETRY			4	/* max num touch data read */
#define INTERRUPT_FLAGS(x)		((x) & 0xC0)>>6
#define TOUCHED_KEYINFO(x)		((x) & 0x0F)
#define GET_NUM_TOUCHES(x)		((x) & 0x0F)
#define IS_LARGE_AREA(x)		(((x) & 0x10) >> 4)

extern void tma340_update_threshold(bool flag);

#endif
