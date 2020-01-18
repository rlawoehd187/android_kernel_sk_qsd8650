/*  $Revision: 1.2 $ 
 */


/*
* Copyright (C) 2009 Bosch Sensortec GmbH
*
* BMA150 calibration routine based on SMB380 API
* 
* Usage: calibration of SMB380/BMA150 acceleration sensor
*
* Author:	Francois.Beauchaud@bosch-sensortec.com
*
* Disclaimer
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


/*! \file BMA150calib.h
    \brief This file contains all function headers for the SMB380/BMA150 calibration process
        
*/


#ifndef __BMA150_CALIBRATION__
#define __BMA150_CALIBRATION__

#include "smb380.h"

#define ORIENT_X				0
#define ORIENT_Y				0
#define ORIENT_Z				1

#define BMA150_CALIBRATION_MAX_TRIES 1000
#define NO_ERR					0x00
#define NO_CALIB				0x100
#define CALIB_ERR_MOV 			0x80
#define CALIB_X_AXIS			1
#define CALIB_Y_AXIS			2
#define CALIB_Z_AXIS			4

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
int bma150_calibrate(smb380acc_t orientation, smb380acc_t *offset);

/** Read the actual calibration register of the sensor
  \param *offset contains the actual sensor calibration values
  \return 0x00 Offset words successful updated
*/
int bma150_readCalib(smb380acc_t *offset);

/** Write the given calibration register in the sensor either in the image registers or the EEPROM
  \param *offset takes the sensor calibration values
  \param EEPROM when EEPROM is 0, hte Image register will be updated. otherwise EEPROM is being written
  \return 0x00 Offset words successful updated
*/
int bma150_writeCalib(smb380acc_t *offset,int EEPROM);


/*** PRIVATE ***/

/** overall calibration process. This function takes care about all other functions 
  \param orientation input for orientation [0;0;1] for measuring the device in horizontal surface up
  \param tries takes the number of wanted iteration steps
  \return 0x00: calibration passed ... 0x80: Movement detected 
*/
int bma150_calibration(smb380acc_t orientation,smb380acc_t *offset, int tries);

/** calculates new offset in respect to acceleration data and old offset register values
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param accel holds the measured acceleration value of one axis
  \param *offset takes the old offset value and modifies it to the new calculated one
  \param stepsize holds the value of the calculated stepsize for one axis
  \param *min_accel_adapt stores the minimal accel_adapt to see if calculations make offset worse
  \param *min_offset stores old offset bevor actual calibration to make resetting possible in next function call
  \return	0x00: Axis was already calibrated
			0x01: Axis needs calibration
			0x11: Axis needs calibration and the calculated offset was outside boundaries
 */
int bma150_calc_new_offset(short orientation, short accel, short *offset, short *stepsize, short *min_accel_adapt, short *min_offset, short *min_reached, short *prev_steps, int iteration_counter);

/** reads out acceleration data and averages them, measures min and max
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param num_avg numer of samples for averaging
  \param *min returns the minimum measured value
  \param *max returns the maximum measured value
  \param *average returns the average value
 */
int bma150_read_accel_avg(int num_avg, smb380acc_t *min, smb380acc_t *max, smb380acc_t *avg );

/** verifies the accerleration values to be good enough for calibration calculations
 \param min takes the minimum measured value
  \param max takes the maximum measured value
  \param takes returns the average value
  \return 1: min,max values are in range, 0: not in range
*/
int bma150_verify_min_max(smb380acc_t min, smb380acc_t max, smb380acc_t avg);

#endif // endif __BMA150_CALIBRATION__
