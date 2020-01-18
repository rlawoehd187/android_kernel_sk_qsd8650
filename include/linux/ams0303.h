//
// Definitions for amosense magnetic field sensor chips (AMS0303H, AMS0303M)
//

#ifndef		AMS0303_H
#define		AMS0303_H

#include <linux/ioctl.h>

#define		AMS_MODEL_TYPE_0303H (1)
#define		AMS_MODEL_TYPE_0303M (2)

// IOCTLs for AMS0303
#define		AMS_MAG_IO												0x50
#define		AMS_IOCTL_MAG_RESET								_IO(AMS_MAG_IO,	1)				// reset chip
#define		AMS_IOCTL_MAG_POWER_UP							_IO(AMS_MAG_IO,	2)				// power up
#define		AMS_IOCTL_MAG_POWER_DOWN						_IO(AMS_MAG_IO,	3)				// power down
#define		AMS_IOCTL_MAG_READ_ID								_IOR(AMS_MAG_IO, 4, int)			// read id
#define		AMS_IOCTL_MAG_READ_REVISION					_IOR(AMS_MAG_IO, 5, int)			// read revision number
#define		AMS_IOCTL_MAG_READ_STATUS						_IOR(AMS_MAG_IO, 6, int)			// read status
#define		AMS_IOCTL_MAG_READ_OTP							_IOR(AMS_MAG_IO, 7, int)			// read otp data
#define		AMS_IOCTL_MAG_READ_DATA						_IOR(AMS_MAG_IO, 8, int[3])		// read data ( [0]=X, [1]=Y, [2]=Z )
#define		AMS_IOCTL_MAG_ENABLE								_IOR(AMS_MAG_IO, 9, int)			// enable sensor (initialize, power up)
#define		AMS_IOCTL_MAG_DISABLE								_IO(AMS_MAG_IO,	10)			// disable sensor (power down)
#define		AMS_IOCTL_FACTORY_TEST							_IOR(AMS_MAG_IO,	11, int)		// for factory test
#define		AMS_IOCTL_MAG_MODEL_TYPE						_IOR(AMS_MAG_IO,	12, int)		


/*
// IOCTLs for amsdaemon
#define		AMS_DAEMON_IOCTL_READ_DATA					_IOR(AMS_MAG_IO, 12, int)						// read data ( [0]=X, [1]=Y, [2]=Z )
#define		AMS_DAEMON_IOCTL_READ_OTP						_IOR(AMS_MAG_IO, 13, unsigned int)			// read otp data
#define		AMS_DAEMON_IOCTL_READ_FLAG					_IOR(AMS_MAG_IO, 14, int)						// read flag
#define		AMS_DAEMON_IOCTL_WRITE_DATA					_IOW(AMS_MAG_IO, 15, int)
#define		AMS_DAEMON_IOCTL_MAG_ENABLE					_IO(AMS_MAG_IO, 16)
#define		AMS_DAEMON_IOCTL_MAG_DISABLE				_IO(AMS_MAG_IO, 17)
#define		AMS_DAEMON_IOCTL_ACC_ENABLE					_IO(AMS_MAG_IO, 18)
#define		AMS_DAEMON_IOCTL_ACC_DISABLE					_IO(AMS_MAG_IO, 19)
#define		AMS_DAEMON_IOCTL_COMPASS_ENABLE			_IO(AMS_MAG_IO, 20)
#define		AMS_DAEMON_IOCTL_COMPASS_DISABLE			_IO(AMS_MAG_IO, 21)
#define		AMS_DAEMON_IOCTL_MAG_READ_ID				_IOR(AMS_MAG_IO, 22, int)
#define		AMS_DAEMON_IOCTL_DRIVER_INIT					_IO(AMS_MAG_IO, 23)
#define		AMS_DAEMON_IOCTL_SELECT_DEVICE				_IOW(AMS_MAG_IO, 24, int)
*/

// status flags
#define		AMS_STATUS_NOT_SATURATION	0x00
#define		AMS_STATUS_SATURATION			0x01




#endif		// AMS0303_H
