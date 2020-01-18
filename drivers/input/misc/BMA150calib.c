/*´ $Revision: 1.2 $ 
 */


/** \mainpage BMA150 / SMB380 In Line Calibration
 * Copyright (C) 2009 Bosch Sensortec GmbH
 *  \section intro_sec Introduction
 * BMA150 / SMB380 3-axis digital Accelerometer In Line Calibration Process
 * This software is compatible with all Bosch Sensortec SMB380 API >=1.1
 *
 * Author:	Francois.Beauchaud@bosch-sensortec.com
 *
 * 
 * \section disclaimer_sec Disclaimer
 *
 *
 *
 * Common:
 * Bosch Sensortec products are developed for the consumer goods industry. They may only be used 
 * within the parameters of the respective valid product data sheet.  Bosch Sensortec products are 
 * provided with the express understanding that there is no warranty of fitness for a particular purpose. 
 * They are not fit for use in life-sustaining, safety or security sensitive systems or any system or device 
 * that may lead to bodily harm or property damage if the system or device malfunctions. In addition, 
 * Bosch Sensortec products are not fit for use in products which interact with motor vehicle systems.  
 * The resale and/or use of products are at the purchaser’s own risk and his own responsibility. The 
 * examination of fitness for the intended use is the sole responsibility of the Purchaser. 
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims, including any claims for 
 * incidental, or consequential damages, arising from any product use not covered by the parameters of 
 * the respective valid product data sheet or not approved by Bosch Sensortec and reimburse Bosch 
 * Sensortec for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products, particularly with regard to 
 * product safety and inform Bosch Sensortec without delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary from the valid 
 * technical specifications of the product series. They are therefore not intended or fit for resale to third 
 * parties or for use in end products. Their sole purpose is internal client testing. The testing of an 
 * engineering sample may in no way replace the testing of a product series. Bosch Sensortec 
 * assumes no liability for the use of engineering samples. By accepting the engineering samples, the 
 * Purchaser agrees to indemnify Bosch Sensortec from all claims arising from the use of engineering 
 * samples.
 *
 * Special:
 * This software module (hereinafter called "Software") and any information on application-sheets 
 * (hereinafter called "Information") is provided free of charge for the sole purpose to support your 
 * application work. The Software and Information is subject to the following terms and conditions: 
 *
 * The Software is specifically designed for the exclusive use for Bosch Sensortec products by 
 * personnel who have special experience and training. Do not use this Software if you do not have the 
 * proper experience or training. 
 *
 * This Software package is provided `` as is `` and without any expressed or implied warranties, 
 * including without limitation, the implied warranties of merchantability and fitness for a particular 
 * purpose. 
 *
 * Bosch Sensortec and their representatives and agents deny any liability for the functional impairment 
 * of this Software in terms of fitness, performance and safety. Bosch Sensortec and their 
 * representatives and agents shall not be liable for any direct or indirect damages or injury, except as 
 * otherwise stipulated in mandatory applicable law.
 * 
 * The Information provided is believed to be accurate and reliable. Bosch Sensortec assumes no 
 * responsibility for the consequences of use of such Information nor for any infringement of patents or 
 * other rights of third parties which may result from its use. No license is granted by implication or 
 * otherwise under any patent or patent rights of Bosch. Specifications mentioned in the Information are 
 * subject to change without notice.
 *
 * It is not allowed to deliver the source code of the Software to any third party without permission of 
 * Bosch Sensortec.
 */

/*! \file BMA150calib.c
    \brief This file contains all function implementatios for the SMB380/BMA150 calibration process
        
*/
	

#include <linux/input.h>

#include "BMA150calib.h"

#define Abs(x) ((x) > 0 ? (x) : -(x))


/*** PUBLIC ***/

/** Calibration routine. This function will run the calibration and return the new calculated offset.
  \param orientation takes the orientation of the sensor. [0;0;1] for measuring the device in horizontal surface up
  \param *offset contains the new calculated offset for each axis
  \return	0x00: Calibration successful
			Combination of:
			0x01: Axis X is not fully calibrated
			0x02: Axis Y is not fully calibrated
			0x04: Axis Z is not fully calibrated
			0x10: X axis trimming registers are too small to reach the full calibration 
			0x20: Y axis trimming registers are too small to reach the full calibration 
			0x40: Z axis trimming registers are too small to reach the full calibration 
				.
			0x80: Calibration aborted, Movement detected.
 */

int bma150_calibrate(smb380acc_t orientation, smb380acc_t *offset)
{		
	return bma150_calibration(orientation,offset,BMA150_CALIBRATION_MAX_TRIES);
}

/** Read the actual calibration register of the sensor
  \param *offset contains the actual sensor calibration values
  \return 0x00 Offset words successful updated
*/
int bma150_readCalib(smb380acc_t *offset)
{
	int comres;
	comres = smb380_set_ee_w(1);
	comres |= smb380_get_offset(0, &(offset->x));
    comres |= smb380_get_offset(1, &(offset->y));
    comres |= smb380_get_offset(2, &(offset->z));
	comres |= smb380_set_ee_w(0);
	return comres;
}

/** Write the given calibration register in the sensor either in the image registers or the EEPROM
  \param *offset takes the sensor calibration values
  \param EEPROM when EEPROM is 0, hte Image register will be updated. otherwise EEPROM is being written
  \return 0x00 Offset words successful updated
*/
int bma150_writeCalib(smb380acc_t *offset,int EEPROM)
{
	int comres;

	comres = smb380_set_ee_w(1);
	if (EEPROM)
	{
		comres |= smb380_set_offset_eeprom(0, offset->x);
		comres |= smb380_set_offset_eeprom(1, offset->y);
		comres |= smb380_set_offset_eeprom(2, offset->z);
	}
	else
	{
		comres = smb380_set_offset(0, offset->x);
		comres |= smb380_set_offset(1, offset->y);
		comres |= smb380_set_offset(2, offset->z);
	}
	comres |= smb380_set_ee_w(0);
	return comres;
}

/*** PRIVATE ***/

/** overall calibration process. This function takes care about all other functions 
  \param orientation input for orientation [0;0;1] for measuring the device in horizontal surface up
  \param *offset output structure for offset values (x, y and z)
  \param *tries takes the number of wanted iteration steps, this pointer returns the number of calculated steps after this routine has finished
  \return 0: calibration passed; {0x1,0x2,0x4}: did not pass within N steps; 0x80 : Too much movement 
*/
int bma150_calibration(smb380acc_t orientation, smb380acc_t *offset, int tries)
{

	short offset_x, offset_y, offset_z;
	int need_calibration=0, min_max_ok=0;
	int error_flag=0;
	int iteration_counter =0;
	int dummy;
	int rtn;

//****************************************************************************	
	unsigned char dummy2;
	short min_accel_adapt_x = 0xFE00, min_accel_adapt_y = 0xFE00, min_accel_adapt_z = 0xFE00;
	short min_offset_x = 0, min_offset_y = 0, min_offset_z = 0; 
	short min_reached_x	= 0, min_reached_y	= 0, min_reached_z	= 0;
	short prev_steps_x = 0x01FF, prev_steps_y = 0x01FF, prev_steps_z = 0x01FF;
//****************************************************************************

	smb380acc_t min,max,avg,gain,step;

	printk(KERN_WARNING	"bma150_calibration in \n");

    smb380_set_range(SMB380_RANGE_2G);
    smb380_set_bandwidth(SMB380_BW_25HZ); 
	smb380_set_ee_w(1);
	smb380_get_offset(0, &offset_x);
    smb380_get_offset(1, &offset_y);
    smb380_get_offset(2, &offset_z);
	

//****************************************************************************
	smb380_read_reg(0x16, &dummy2, 1);			/* read gain registers from image */
	gain.x = dummy2 & 0x3F;
	smb380_read_reg(0x17, &dummy2, 1);
	gain.y = dummy2 & 0x3F;
	smb380_read_reg(0x18, &dummy2, 1);
	gain.z = dummy2 & 0x3F;
	smb380_set_ee_w(0);

	step.x = gain.x * 15/64 + 7;				/* calculate stepsizes for all 3 axes */
	step.y = gain.y * 15/64 + 7;
	step.z = gain.z * 15/64 + 7;

	smb380_pause(50);							/* needed to prevent CALIB_ERR_MOV */
//***************************************************************************

	printk(KERN_WARNING	"calibrate old : x=%d, y=%d, z=%d \n", offset_x, offset_y, offset_z);
	printk(KERN_WARNING	"gain old : x=%d, y=%d, z=%d \n", gain.x, gain.y, gain.z);

	do {

		printk(KERN_WARNING	"do routine \n");

		bma150_read_accel_avg(10, &min, &max, &avg);  		/* read acceleration data min, max, avg */

		min_max_ok = bma150_verify_min_max(min, max, avg);

		if (!min_max_ok)										/* check if calibration is possible */
		{
			iteration_counter++;
			continue;
			//return CALIB_ERR_MOV;
		}

		need_calibration = 0;

		/* x-axis */
		dummy = bma150_calc_new_offset(orientation.x, avg.x, &offset_x, &step.x, &min_accel_adapt_x, &min_offset_x, &min_reached_x, &prev_steps_x, iteration_counter);
		
		smb380_set_ee_w(1);
		if (dummy & 0x01)									/* x-axis calibrated ? */
		{					
			printk(KERN_WARNING	"calibrate new : x=%d\n", offset_x);
			smb380_set_offset(0, offset_x);
			need_calibration = CALIB_X_AXIS;
		}
		if (dummy & 0x10)									/* x-axis offset register boundary reached */
			error_flag |= (CALIB_X_AXIS << 4);

		
		/* y-axis */
		dummy = bma150_calc_new_offset(orientation.y, avg.y, &offset_y, &step.y, &min_accel_adapt_y, &min_offset_y, &min_reached_y, &prev_steps_y, iteration_counter);
		
		if (dummy & 0x01)									/* y-axis calibrated ? */
		{
			printk(KERN_WARNING	"calibrate new : y=%d\n", offset_y);
			smb380_set_offset(1, offset_y);
			need_calibration |= CALIB_Y_AXIS;
		}
		if (dummy & 0x10)									/* y-axis offset register boundary reached ? */
			error_flag |= (CALIB_Y_AXIS << 4);

		
		/* z-axis */
		dummy = bma150_calc_new_offset(orientation.z, avg.z, &offset_z, &step.z, &min_accel_adapt_z, &min_offset_z, &min_reached_z, &prev_steps_z, iteration_counter);
		if (dummy & 0x01)									/* z-axis calibrated ? */
		{
			printk(KERN_WARNING	"calibrate new : z=%d\n", offset_z);
			smb380_set_offset(2, offset_z);
			need_calibration |= CALIB_Z_AXIS;
		}
		if (dummy & 0x10)									/* z-axis offset register boundary reached */
			error_flag |= (CALIB_Z_AXIS << 4);
		

		smb380_set_ee_w(0);
		iteration_counter++;

		if (need_calibration)	/* if one of the offset got changed wait for the filter to fill with the new acceleration */
		{
			smb380_pause(50);
   		}
    } while (need_calibration && (iteration_counter != tries));

	offset->x = offset_x;
	offset->y = offset_y;
	offset->z = offset_z;

#if 0	
	if(iteration_counter == 1)					/* no calibration needed at all */
	{
		error_flag |= NO_CALIB;
		return error_flag;
	}
	else if (iteration_counter == tries)		/* further calibration needed */
	{
		error_flag |= need_calibration;
		return error_flag;
	}

	rtn = bma150_writeCalib(offset, 1);
	smb380_pause(50);

	if (rtn != 0)
	{
		printk(KERN_WARNING	"bma150_calibration ee write FAIL \n");	
		return rtn;
	}
#endif
	if(iteration_counter == 1)					/* no calibration needed at all */
	{
		rtn = NO_ERR;
	}
	else if (iteration_counter >= tries)		/* further calibration needed */
	{
		error_flag |= need_calibration;
		rtn = error_flag;
	}
	else
	{	
		rtn = bma150_writeCalib(offset, 1);
		smb380_pause(50);

		if (rtn != 0)
		{
			printk(KERN_WARNING	"bma150_calibration ee write FAIL \n");	
		}
		else
		{
			rtn = NO_ERR;
		}
	}
	
	smb380_soft_reset();
	smb380_pause(50);
	
	smb380_set_range(SMB380_RANGE_2G);
    smb380_set_bandwidth(SMB380_BW_25HZ); 
	
	//return NO_ERR;
	return rtn;
}

/** calculates new offset in respect to acceleration data and old offset register values
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param accel holds the measured acceleration value of one axis
  \param *offset takes the old offset value and modifies it to the new calculated one
  \param stepsize holds the value of the calculated stepsize for one axis
  \param *min_accel_adapt stores the minimal accel_adapt to see if calculations make offset worse
  \param *min_offset stores old offset bevor actual calibration to make resetting possible in next function call
  \param *min_reached stores value to prevent further calculations if minimum for an axis reached 
  \return	0x00: Axis was already calibrated
			0x01: Axis needs calibration
			0x11: Axis needs calibration and the calculated offset was outside boundaries
 */
int bma150_calc_new_offset(short orientation, short accel, short *offset, short *stepsize, short *min_accel_adapt, short *min_offset, short *min_reached, short *prev_steps, int iteration_counter)
{
	short old_offset;
	short new_offset;
	short accel_adapt;
	short steps=0;

   	unsigned char  calibrated = 0;

   	old_offset = *offset;
   
   	accel_adapt = accel - (orientation * 256);
	if(accel_adapt < -512)
	{
	 	accel_adapt = -512;
	}
	else if(accel_adapt > 511)
	{
	 	accel_adapt = 511;
	}
		                                
   	//if (((accel_adapt > 7) || (accel_adapt < -7)) && !(*min_reached))	/* does the axis need calibration? minimum for this axis not yet reached? */
	if (((accel_adapt > 4) || (accel_adapt < -4)) && !(*min_reached))
   	{
		if (Abs(accel_adapt) <= Abs(*min_accel_adapt))				/* accel_adapt smaller than minimal accel_adapt ?
															   			means: previous offset calculation lead to better result */
		{
			if(((3*(*prev_steps) * (*stepsize)) <= 2*(Abs((*min_accel_adapt) - (accel_adapt)))) && (iteration_counter >= 1))/* if calculated stepsize is too small compared to real stepsize*/
			{
				(*stepsize) = (Abs((*min_accel_adapt) - (accel_adapt))) / (*prev_steps);	/* adapt stepsize */
			}
						
			if ((accel_adapt < (*stepsize)) && (accel_adapt > 0))	/* check for values less than quantisation of offset register */
				new_offset = old_offset -1;          
		 	else if ((accel_adapt > -(*stepsize)) && (accel_adapt < 0))    
		   		new_offset = old_offset +1;
	     	else
			{
				steps = (accel_adapt/(*stepsize));					/* calculate offset LSB */
				if((2*(Abs(steps * (*stepsize) - accel_adapt))) > (*stepsize))	/* for better performance (example: accel_adapt = -25, stepsize = 13 --> two steps better than one) */
				{
					if(accel_adapt < 0)
						steps-=1;
					else
						steps+=1;	
				} 
	       		new_offset = old_offset - steps;					/* calculate new offset */
			}
	     	
			if (new_offset < 0)										/* check for register boundary */
			{
				new_offset = 0;										/* <0 ? */
				calibrated = 0x10;
			}
	     	else if (new_offset > 1023)
		 	{
				new_offset = 1023;									/* >1023 ? */
				calibrated = 0x10;
		 	}

			*prev_steps = Abs(steps);								/* store number of steps */
			if(*prev_steps==0)										/* at least 1 step is done */
				*prev_steps = 1;
						
			*min_accel_adapt = accel_adapt; 						/* set current accel_adapt value as minimum */
			*min_offset = old_offset;								/* store old offset (value before calculations above) */

			*offset = new_offset;									/* set offset as calculated */
		}
		else
		{
			*offset = *min_offset;									/* restore old offset */
			if((2*(*prev_steps) * (*stepsize)) <= (Abs((*min_accel_adapt) - (accel_adapt))))/* if calculated stepsize is too small compared to real stepsize*/
			{
				(*stepsize) = (Abs((*min_accel_adapt) - (accel_adapt))) / (*prev_steps);	/* adapt stepsize */
			}
			else
			{
				*min_reached = 0x01;								/* prevent further calibrations */
			}	
		}
					    
	 	calibrated |= 0x01;
   	}
  	return calibrated;
}

/** reads out acceleration data and averages them, measures min and max
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param num_avg numer of samples for averaging
  \param *min returns the minimum measured value
  \param *max returns the maximum measured value
  \param *average returns the average value
 */
int bma150_read_accel_avg(int num_avg, smb380acc_t *min, smb380acc_t *max, smb380acc_t *avg )
{
   	long x_avg=0, y_avg=0, z_avg=0;   
   	int comres=0;
   	int i;
   	smb380acc_t accel;		                /* read accel data */

   	x_avg = 0; y_avg=0; z_avg=0;                  
   	max->x = -512; max->y =-512; max->z = -512;
   	min->x = 512;  min->y = 512; min->z = 512;  

	for (i=0; i<num_avg; i++) {
		comres += smb380_read_accel_xyz(&accel);      /* read 10 acceleration data triples */
		if (accel.x > max->x)
			max->x = accel.x;
		if (accel.x < min->x) 
			min->x = accel.x;

		if (accel.y > max->y)
			max->y = accel.y;
		if (accel.y < min->y) 
			min->y = accel.y;

		if (accel.z > max->z)
			max->z = accel.z;
		if (accel.z < min->z) 
			min->z = accel.z;
		
		x_avg += accel.x;
		y_avg += accel.y;
		z_avg += accel.z;

		smb380_pause(10);
	}
	avg->x = (x_avg / num_avg);                             /* calculate averages, min and max values */
	avg->y = (y_avg / num_avg);
	avg->z = (z_avg / num_avg);
	return comres;
}
	 

/** verifies the accerleration values to be good enough for calibration calculations
  \param min takes the minimum measured value
  \param max takes the maximum measured value
  \param takes returns the average value
  \return 1: min,max values are in range, 0: not in range
*/
int bma150_verify_min_max(smb380acc_t min, smb380acc_t max, smb380acc_t avg) 
{
	short dx, dy, dz;
	int ver_ok=1;

	dx =  ((max.x) - (min.x));    /* calc delta max-min */
	dy =  ((max.y) - (min.y));
	dz =  ((max.z) - (min.z));


	if ((dx> 10) || (dx<-10)) 
	  ver_ok = 0;
    if ((dy> 10) || (dy<-10)) 
	  ver_ok = 0;
	if ((dz> 10) || (dz<-10)) 
	  ver_ok = 0;

	return ver_ok;
}

	

