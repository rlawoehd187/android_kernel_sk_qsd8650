/* arch/arm/mach-msm/board-s1-keypad.c
 *
 * Copyright (C) 2008 SK Telesys, Inc.
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

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/board-s1.h>


static struct gpio_event_direct_entry s1_keypad_map[] = {
#if (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT01)
	{ S1_GPIO_HOLD_KEY,           KEY_0     },
	{ S1_GPIO_HOME_KEY,           KEY_UP    },
	{ S1_GPIO_SEND_KEY,           KEY_MENU  },
	{ S1_GPIO_MENU_KEY,           KEY_DOWN  },
	{ S1_GPIO_VOLUME_DOWN_KEY,    KEY_RIGHT },
	{ S1_GPIO_VOLUME_UP_KEY,      KEY_LEFT  },
	{ S1_GPIO_END_KEY,            KEY_BACK  },
	{ S1_GPIO_CLEAR_KEY,          232       }
#elif (CONFIG_S1_BOARD_VER == S1_BOARD_VER_PT02)
	{ S1_GPIO_HOLD_KEY,           KEY_LEFT  },
	{ S1_GPIO_HOME_KEY,           KEY_DOWN  },
	{ S1_GPIO_SEND_KEY,           KEY_MENU  },
	{ S1_GPIO_MENU_KEY,           KEY_RIGHT },
	{ S1_GPIO_VOLUME_DOWN_KEY,    232       },
	{ S1_GPIO_VOLUME_UP_KEY,      230  },
	{ S1_GPIO_END_KEY,            KEY_BACK  },
	{ S1_GPIO_CLEAR_KEY,          KEY_UP    }
#else
	/* Change key map to UI proposal */

	{ S1_GPIO_HOME_KEY,           KEY_F23        },	/* F23 */
	{ S1_GPIO_VOLUME_DOWN_KEY,    KEY_VOLUMEDOWN },
	{ S1_GPIO_VOLUME_UP_KEY,      KEY_VOLUMEUP   },
	{ S1_GPIO_SEARCH_KEY,         KEY_F24        }  /* F24 */
/*
	{ S1_GPIO_HOME_KEY,           230            },
	{ S1_GPIO_VOLUME_DOWN_KEY,    KEY_VOLUMEDOWN },
	{ S1_GPIO_VOLUME_UP_KEY,      KEY_VOLUMEUP   },
	{ S1_GPIO_SEARCH_KEY,         KEY_SEARCH     }
*/	
#endif
};

static u32 gpio_key_tlmm[] =
{
#if (CONFIG_S1_BOARD_VER < S1_BOARD_VER_WS01)	
	GPIO_CFG(S1_GPIO_HOLD_KEY,        0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_HOME_KEY,        0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SEND_KEY,        0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_MENU_KEY,        0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_VOLUME_DOWN_KEY, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_VOLUME_UP_KEY,   0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_END_KEY,         0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_CLEAR_KEY,       0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA)
#else
	GPIO_CFG(S1_GPIO_HOME_KEY,        0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_VOLUME_DOWN_KEY, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_VOLUME_UP_KEY,   0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),
	GPIO_CFG(S1_GPIO_SEARCH_KEY,      0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA)
#endif
};

static struct gpio_event_input_info s1_keypad_navi_info = {
	.info.func = gpio_event_input_func,
	.flags = GPIOEDF_PRINT_KEYS,
	.type = EV_KEY,
	.keymap = s1_keypad_map,
	.debounce_time.tv.nsec = 20 * NSEC_PER_MSEC,
	//.poll_time.tv.nsec = 40 * NSEC_PER_MSEC,
	.keymap_size = ARRAY_SIZE(s1_keypad_map)
};

static struct gpio_event_info *s1_keypad_info[] = {
	&s1_keypad_navi_info.info
};

static struct gpio_event_platform_data s1_keypad_data = {
	.name = "s1_keypad",
	.info = s1_keypad_info,
	.info_count = ARRAY_SIZE(s1_keypad_info)
};

static struct platform_device s1_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id   = -1,
	.dev  = {
		.platform_data = &s1_keypad_data,
	},
};


static int __init s1_init_keypad(void)
{
	
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(gpio_key_tlmm); i++) {
		rc = gpio_tlmm_config(gpio_key_tlmm[i], GPIO_ENABLE);
		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, gpio_key_tlmm[i], rc);
	}
	
	return platform_device_register(&s1_keypad_device);
}

device_initcall(s1_init_keypad);

