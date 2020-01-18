#ifndef _CAMSENSORG_TUNE_H_
#define _CAMSENSORG_TUNE_H_


#include "isx006.h"
#include "mt9v113.h"


// If below feature is enabled, whole tunning functionality is enabled.
//#define FEATUE_SKTS_CAM_SENSOR_TUNNING


#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
//#define ISX006_INIT_REG_FILE      "/data/isx006_init.txt"
#define ISX006_INIT_REG_FILE        "/sdcard/isx006_init.txt"
#define ISX006_INIT_REG_MAX         1000
#define ISX006_INIT_REG_2_FILE      "/sdcard/isx006_init_2.txt"
#define ISX006_INIT_REG_2_MAX       4000
#else
#define ISX006_INIT_REG_FILE        "/sdcard/isx006_init.txt"
#define ISX006_INIT_REG_MAX         4000
#endif
#define ISX006_SCENE_REG_FILE       "/sdcard/isx006_scene.txt"
#define ISX006_SCENE_REG_MAX        77 // 7*11
#define ISX006_SCENE_TBL_SIZE       7

#define MT9V113_INIT_REG_FILE       "/sdcard/mt9v113_init.txt"
#define MT9V113_INIT_REG_MAX        600
#define MT9V113_INIT_VT_REG_FILE    "/sdcard/mt9v113_init_vt.txt"
#define MT9V113_INIT_VT_REG_MAX     600


extern int camsensor_set_file_read(char *filename, uint32_t *tune_tbl, uint16_t *tune_tbl_size);


#endif // _CAMSENSORG_TUNE_H_
