#ifndef __LINUX_I2C_TMG200_H
#define __LINUX_I2C_TMG200_H

/* linux/i2c/tmg200.h */

struct TouchI2CReg {
	uint8_t bMode; // R/W
	uint8_t bGestureID; // Read Only
	uint8_t bDirection; // Read Only
	uint16_t wX1Current; // Read Only
	uint16_t wY1Current; // Read Only
	uint16_t wX2Current; // Read Only
	uint16_t wY2Current; // Read Only
	uint8_t bSensorMask1; // Read Only, bit[7:6]=Y2~Y1, bit[5:0]=X6~X0
	uint8_t bSensorMask2; // Read Only, bit[7:0]=Y10~Y3
	uint16_t wXFirst; // Read Only
	uint16_t wYFirst; // Read Only    
	uint8_t bProjectInfo; // 0x01:K2
	uint8_t bRevisionInfo; // Read Only, bit[7:4]=Major, bit[3:0]=Minor
	uint8_t bLowLevelRevisionInfo; // low level firmware version from Japan Cypress
};

struct tmg200_platform_data {
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

#endif
