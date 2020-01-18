#include <linux/types.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include "camsensor_tune.h"


#if defined(FEATUE_SKTS_CAM_SENSOR_TUNNING)


uint8_t *tune_file_buf;

struct isx006_i2c_reg_conf  isx006_tune_init[ISX006_INIT_REG_MAX];
#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
struct isx006_i2c_reg_conf  isx006_tune_init_2[ISX006_INIT_REG_2_MAX];
#endif
struct isx006_i2c_reg_conf  isx006_tune_scene[ISX006_SCENE_REG_MAX];

struct mt9v113_i2c_reg_conf mt9v113_tune_init[MT9V113_INIT_REG_MAX];
struct mt9v113_i2c_reg_conf mt9v113_tune_init_vt[MT9V113_INIT_VT_REG_MAX];


static uint8_t *find_next_register_16bit(uint8_t *ptr_address, uint16_t *address);
static uint8_t *find_next_string(uint8_t *ptr_address, uint8_t *string);
static uint16_t string_to_num(uint8_t *string);

#define STRING_LEN  10


// file I/O reference : android/kernel/drivers/video/msm/logo.c
int camsensor_set_file_read(char *filename, uint32_t *tune_tbl_ptr, uint16_t *tune_tbl_size)
{
	mm_segment_t old_fs;
	int fd;
	int err = 0;
	uint32_t file_size, byte_read;
	uint8_t *string_ptr;
	uint16_t reg_address, reg_val, reg_width;
	uint32_t reg_cnt = 0;


    old_fs = get_fs();
    set_fs(KERNEL_DS);
	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0)
	{
		printk(KERN_ERR "%s: Can not open %s (fd=%d)\n", __func__, filename, fd);
		return -ENOENT;
	}
	
	file_size = sys_lseek(fd, (off_t)0, SEEK_END);
	if (file_size == 0)
	{
		err = -EIO;
		goto err_close_file;
	}

	sys_lseek(fd, (off_t)0, SEEK_SET);
	
	tune_file_buf = kmalloc(file_size + 2, GFP_KERNEL);
	if (!tune_file_buf)
	{
		printk(KERN_ERR "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_close_file;
	}	

	*(tune_file_buf + file_size)     = 0;
	*(tune_file_buf + file_size + 1) = 0;
	
	byte_read = sys_read(fd, (char *)tune_file_buf, file_size);
	if (byte_read != file_size)
	{
		printk(KERN_ERR "%s: sys_read error -> file_size = %d, byte_read = %d\n", __func__, file_size, byte_read);
		err = -EIO;
		goto err_free_data;
	}

	string_ptr = tune_file_buf;

	if (strcmp(filename, ISX006_INIT_REG_FILE) == 0)
    {
        memset(&isx006_tune_init[0], 0x0, sizeof(isx006_tune_init[0]));
        while(string_ptr)
        {
            if ((string_ptr = find_next_register_16bit(string_ptr, &reg_address)) != 0)
            {
                if ((string_ptr = find_next_register_16bit(string_ptr, &reg_val)) != 0)
                {
                    if ((string_ptr = find_next_register_16bit(string_ptr, &reg_width)) != 0)
                    {
      	                isx006_tune_init[reg_cnt].waddr = reg_address;
    	                isx006_tune_init[reg_cnt].wdata = reg_val;
    	                isx006_tune_init[reg_cnt].width = reg_width;
    	                reg_cnt++;
    	                if (reg_cnt > ISX006_INIT_REG_MAX)
    	                {
    	                    printk(KERN_ERR "%s: error! exceeded tune max buffer size (reg_cnt = %d)\n", __func__, reg_cnt);
    	                    err = -ENOMEM;
    	                    goto err_free_data;
    	                }
        	        }   
              	}
            }
        }
        printk(KERN_ERR "%s: address of isx006_tune_init = %p\n", __func__, &isx006_tune_init[0]);
        *tune_tbl_ptr = (uint32_t)&isx006_tune_init[0];
        *tune_tbl_size = reg_cnt;   
    }
#if defined(FEATURE_SKTS_CAM_SENSOR_SPLIT_INIT_TABLE)
	else if (strcmp(filename, ISX006_INIT_REG_2_FILE) == 0)
    {
        memset(&isx006_tune_init_2[0], 0x0, sizeof(isx006_tune_init_2[0]));
        while(string_ptr)
        {
            if ((string_ptr = find_next_register_16bit(string_ptr, &reg_address)) != 0)
            {
                if ((string_ptr = find_next_register_16bit(string_ptr, &reg_val)) != 0)
                {
                    if ((string_ptr = find_next_register_16bit(string_ptr, &reg_width)) != 0)
                    {
      	                isx006_tune_init_2[reg_cnt].waddr = reg_address;
    	                isx006_tune_init_2[reg_cnt].wdata = reg_val;
    	                isx006_tune_init_2[reg_cnt].width = reg_width;
    	                reg_cnt++;
    	                if (reg_cnt > ISX006_INIT_REG_2_MAX)
    	                {
    	                    printk(KERN_ERR "%s: error! exceeded tune max buffer size (reg_cnt = %d)\n", __func__, reg_cnt);
    	                    err = -ENOMEM;
    	                    goto err_free_data;
    	                }
        	        }   
              	}
            }
        }
        printk(KERN_ERR "%s: address of isx006_tune_init_2 = %p\n", __func__, &isx006_tune_init_2[0]);
        *tune_tbl_ptr = (uint32_t)&isx006_tune_init_2[0];
        *tune_tbl_size = reg_cnt;   
    }
#endif
	else if (strcmp(filename, ISX006_SCENE_REG_FILE) == 0)
    {
        memset(&isx006_tune_scene[0], 0x0, sizeof(isx006_tune_scene[0]));
        while(string_ptr)
        {
            if ((string_ptr = find_next_register_16bit(string_ptr, &reg_address)) != 0)
            {
                if ((string_ptr = find_next_register_16bit(string_ptr, &reg_val)) != 0)
                {
                    if ((string_ptr = find_next_register_16bit(string_ptr, &reg_width)) != 0)
                    {
      	                isx006_tune_scene[reg_cnt].waddr = reg_address;
    	                isx006_tune_scene[reg_cnt].wdata = reg_val;
    	                isx006_tune_scene[reg_cnt].width = reg_width;
    	                reg_cnt++;
    	                if (reg_cnt > ISX006_SCENE_REG_MAX)
    	                {
    	                    printk(KERN_ERR "%s: error! exceeded tune max buffer size (reg_cnt = %d)\n", __func__, reg_cnt);
    	                    err = -ENOMEM;
    	                    goto err_free_data;
    	                }
        	        }   
              	}
            }
        }
        printk(KERN_ERR "%s: address of isx006_tune_scene = %p\n", __func__, &isx006_tune_scene[0]);
        *tune_tbl_ptr = (uint32_t)&isx006_tune_scene[0];
        *tune_tbl_size = reg_cnt;   
    }
    else if (strcmp(filename, MT9V113_INIT_REG_FILE) == 0)
    {
        memset(&mt9v113_tune_init[0], 0x0, sizeof(mt9v113_tune_init[0]));
        while(string_ptr)
        {
            if ((string_ptr = find_next_register_16bit(string_ptr, &reg_address)) != 0)
            {
                if ((string_ptr = find_next_register_16bit(string_ptr, &reg_val)) != 0)
                {
  	                mt9v113_tune_init[reg_cnt].waddr = reg_address;
	                mt9v113_tune_init[reg_cnt].wdata = reg_val;
	                reg_cnt++;
	                if (reg_cnt > MT9V113_INIT_REG_MAX)
	                {
	                    printk(KERN_ERR "%s: error! exceeded tune max buffer size (reg_cnt = %d)\n", __func__, reg_cnt);
	                    err = -ENOMEM;
	                    goto err_free_data;
	                }
              	}
            }
        }
        printk(KERN_ERR "%s: address of mt9v113_tune_init = %p\n", __func__, &mt9v113_tune_init[0]);
        *tune_tbl_ptr = (uint32_t)&mt9v113_tune_init[0];
        *tune_tbl_size = reg_cnt;   
    }
    else if (strcmp(filename, MT9V113_INIT_VT_REG_FILE) == 0)
    {
        memset(&mt9v113_tune_init_vt[0], 0x0, sizeof(mt9v113_tune_init_vt[0]));
        while(string_ptr)
        {
            if ((string_ptr = find_next_register_16bit(string_ptr, &reg_address)) != 0)
            {
                if ((string_ptr = find_next_register_16bit(string_ptr, &reg_val)) != 0)
                {
  	                mt9v113_tune_init_vt[reg_cnt].waddr = reg_address;
	                mt9v113_tune_init_vt[reg_cnt].wdata = reg_val;
	                reg_cnt++;
	                if (reg_cnt > MT9V113_INIT_VT_REG_MAX)
	                {
	                    printk(KERN_ERR "%s: error! exceeded tune max buffer size (reg_cnt = %d)\n", __func__, reg_cnt);
	                    err = -ENOMEM;
	                    goto err_free_data;
	                }
              	}
            }
        }
        printk(KERN_ERR "%s: address of mt9v113_tune_init_vt = %p\n", __func__, &mt9v113_tune_init_vt[0]);
        *tune_tbl_ptr = (uint32_t)&mt9v113_tune_init_vt[0];
        *tune_tbl_size = reg_cnt;   
    }


err_free_data:
	kfree(tune_file_buf);
	
err_close_file:
	sys_close(fd);
	set_fs(old_fs);
	
	return err;
}


static uint8_t *find_next_register_16bit(uint8_t *ptr_address, uint16_t *address)
{
	uint8_t *current_ptr;
	uint8_t string[STRING_LEN];
	
	memset(string, 0x0, sizeof(string));
	current_ptr = find_next_string(ptr_address, string);
	if(current_ptr)
		*address = (uint16_t)string_to_num(string);
	return current_ptr;
}


static uint8_t *find_next_string(uint8_t *ptr_address, uint8_t *string)
{
	uint8_t *current_ptr;
	uint8_t temp_ptr;
	uint16_t idx = 0;

	current_ptr = ptr_address;
	temp_ptr = *current_ptr;
	memset(string, 0x0, STRING_LEN);

	while(temp_ptr)
	{
		if(temp_ptr == 0x2F) // 0x2F : "/"
		{
			if(*(current_ptr+1) == 0x2F)
			{
				current_ptr = (uint8_t *)strchr((char *)current_ptr, (int)0x0A); // 0x0A : LF "\n"
				if(idx > 0)
					break;
			}
			else
			{
				string[idx++] = *current_ptr;
			}
		}
		else if(temp_ptr == 0x23) // 0x23 : "#"
		{
            current_ptr = (uint8_t *)strchr((char *)current_ptr, (int)0x0A); // 0x0A : LF "\n"
		}
		// 0x20 : Space, 0x09 : H tab, 0x0A : LF, 0x0D : CR, 0x2C : ",", 0x7B : "{", 0x7D : "}"
		else if(temp_ptr == 0x20 || temp_ptr == 0x09 || temp_ptr == 0x0A || temp_ptr == 0x0D || temp_ptr == 0x2C || temp_ptr == 0x7B || temp_ptr == 0x7D)
		{
			if(idx > 0 )
				break;
		}
		else
		{
			string[idx++] = *current_ptr;
		}

        current_ptr++;
		temp_ptr = *current_ptr;
	}

	if(idx)
		return current_ptr;
	else
		return NULL;
}


static uint16_t string_to_num(uint8_t *string)
{
	uint8_t mulifier, temp;
	uint16_t subtraction, output = 0;
  
	if(!string || *string == 0)
		return 0;

	if(*string=='0' && (*(string+1)=='X' || *(string+1)=='x'))
	{
		mulifier = 16;
		string += 2;
	}
	else if(*string == '0' && (*(string+1) == 'B' || *(string+1) == 'b'))
	{
		mulifier  = 2;
		string +=2;
	}
	else
	{
		mulifier = 10;
	}

	while((temp = *string) != 0)
	{
		if(temp < 0x3A)
		{
		  subtraction = 0x2F + 1;
		}
		else if(temp < 0x47)
		{
		  subtraction = 'A' - 10;
		}
		else if(temp < 0x67)
		{
		  subtraction = 'a' - 10;
		}
		else
		{
			return 0;
		}
	
		output = output * mulifier + (*(string++) - subtraction);
	}

	return output;
}


#endif // FEATUE_SKTS_CAM_SENSOR_TUNNING
