/*
 *  S1 H2W device detection driver.
 *
 * Copyright (C) 
 * Copyright (C) 
 *
 * Authors:
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <asm/atomic.h>
#include <mach/board.h>
#include <mach/vreg.h>
#include <asm/mach-types.h>
#include <mach/board-s1.h>

#include <mach/msm_rpcrouter.h>
#include <mach/rpc_server_handset.h>
#include "proc_comm.h"

#ifdef CONFIG_DEBUG_S1_H2W
#define H2W_DBG(fmt, arg...) printk(KERN_DEBUG "[H2W] %s " fmt "\n", __FUNCTION__, ## arg)
#else
#define H2W_DBG(fmt, arg...) do {} while (0)
#endif


#define HS_SERVER_PROG 0x30000062
#define HS_SERVER_VERS 0x00010001

#define HS_RPC_PROG 0x30000091

#define HS_PROCESS_CMD_PROC 0x02

#define HS_SUBSCRIBE_SRVC_PROC 0x03
#define HS_REPORT_EVNT_PROC    0x05
#define HS_EVENT_CB_PROC	1
#define HS_EVENT_DATA_VER	1

#define RPC_KEYPAD_NULL_PROC 0
#define RPC_KEYPAD_PASS_KEY_CODE_PROC 2
#define RPC_KEYPAD_SET_PWR_KEY_STATE_PROC 3

#define HS_PWR_K		0x6F	/* Power key */
#define HS_END_K		0x51	/* End key or Power key */
#define HS_STEREO_HEADSET_K	0x82
#define HS_HEADSET_SWITCH_K	0x84
#define HS_REL_K		0xFF	/* key release */

#define KEY(hs_key, input_key) ((hs_key << 24) | input_key)

enum hs_event {
	HS_EVNT_EXT_PWR = 0,	/* External Power status        */
	HS_EVNT_HSD,		/* Headset Detection            */
	HS_EVNT_HSTD,		/* Headset Type Detection       */
	HS_EVNT_HSSD,		/* Headset Switch Detection     */
	HS_EVNT_KPD,
	HS_EVNT_FLIP,		/* Flip / Clamshell status (open/close) */
	HS_EVNT_CHARGER,	/* Battery is being charged or not */
	HS_EVNT_ENV,		/* Events from runtime environment like DEM */
	HS_EVNT_REM,		/* Events received from HS counterpart on a
				remote processor*/
	HS_EVNT_DIAG,		/* Diag Events  */
	HS_EVNT_LAST,		 /* Should always be the last event type */
	HS_EVNT_MAX		/* Force enum to be an 32-bit number */
};

enum hs_src_state {
	HS_SRC_STATE_UNKWN = 0,
	HS_SRC_STATE_LO,
	HS_SRC_STATE_HI,
};

struct hs_event_data {
	uint32_t	ver;		/* Version number */
	enum hs_event	event_type;     /* Event Type	*/
	enum hs_event	enum_disc;     /* discriminator */
	uint32_t	data_length;	/* length of the next field */
	enum hs_src_state	data;    /* Pointer to data */
	uint32_t	data_size;	/* Elements to be processed in data */
};

enum hs_return_value {
	HS_EKPDLOCKED     = -2,	/* Operation failed because keypad is locked */
	HS_ENOTSUPPORTED  = -1,	/* Functionality not supported */
	HS_FALSE          =  0, /* Inquired condition is not true */
	HS_FAILURE        =  0, /* Requested operation was not successful */
	HS_TRUE           =  1, /* Inquired condition is true */
	HS_SUCCESS        =  1, /* Requested operation was successful */
	HS_MAX_RETURN     =  0x7FFFFFFF/* Force enum to be a 32 bit number */
};

struct hs_key_data {
	uint32_t ver;        /* Version number to track sturcture changes */
	uint32_t code;       /* which key? */
	uint32_t parm;       /* key status. Up/down or pressed/released */
};

enum hs_subs_srvc {
	HS_SUBS_SEND_CMD = 0, /* Subscribe to send commands to HS */
	HS_SUBS_RCV_EVNT,     /* Subscribe to receive Events from HS */
	HS_SUBS_SRVC_MAX
};

enum hs_subs_req {
	HS_SUBS_REGISTER,    /* Subscribe   */
	HS_SUBS_CANCEL,      /* Unsubscribe */
	HS_SUB_STATUS_MAX
};

enum hs_event_class {
	HS_EVNT_CLASS_ALL = 0, /* All HS events */
	HS_EVNT_CLASS_LAST,    /* Should always be the last class type   */
	HS_EVNT_CLASS_MAX
};

enum hs_cmd_class {
	HS_CMD_CLASS_LCD = 0, /* Send LCD related commands              */
	HS_CMD_CLASS_KPD,     /* Send KPD related commands              */
	HS_CMD_CLASS_LAST,    /* Should always be the last class type   */
	HS_CMD_CLASS_MAX
};

/*
 * Receive events or send command
 */
union hs_subs_class {
	enum hs_event_class	evnt;
	enum hs_cmd_class	cmd;
};

struct hs_subs {
	uint32_t                ver;
	enum hs_subs_srvc	srvc;  /* commands or events */
	enum hs_subs_req	req;   /* subscribe or unsubscribe  */
	uint32_t		host_os;
	enum hs_subs_req	disc;  /* discriminator    */
	union hs_subs_class      id;
};

struct hs_event_cb_recv {
	uint32_t cb_id;
	uint32_t hs_key_data_ptr;
	struct hs_key_data key;
};

static const uint32_t hs_key_map[] = {
	KEY(HS_PWR_K, KEY_POWER),
	KEY(HS_END_K, KEY_END),
	0
};

enum {
	NO_DEVICE    = 0,
	S1_HEADSET   = 1,  /* with mic    */
	S1_HEADPHONE = 2,  /* without mic */
};
/* Add newer versions at the top of array */
static const unsigned int rpc_vers[] = {
	0x00030001,
	0x00020001,
	0x00010001,
};
struct h2w_info {
	struct switch_dev sdev;
	struct input_dev *input;

	unsigned int irq_jack;
	unsigned int irq_btn;

	struct hrtimer jack_timer;
	ktime_t jack_debounce_time;

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)	
	struct hrtimer btn_timer;
	ktime_t btn_debounce_time;
#endif
};
static struct h2w_info *hi;
static struct msm_rpc_client *rpc_client;

static int hs_find_key(uint32_t hscode)
{
	int i, key;

	key = KEY(hscode, 0);

	for (i = 0; hs_key_map[i] != 0; i++) {
		if ((hs_key_map[i] & 0xff000000) == key)
			return hs_key_map[i] & 0x00ffffff;
	}
	return -1;
}

/*
 * tuple format: (key_code, key_param)
 *
 * old-architecture:
 * key-press = (key_code, 0)
 * key-release = (0xff, key_code)
 *
 * new-architecutre:
 * key-press = (key_code, 0)
 * key-release = (key_code, 0xff)
 */
static void report_hs_key(uint32_t key_code, uint32_t key_parm)
{
	int key, temp_key_code;

	if (key_code == HS_REL_K)
		key = hs_find_key(key_parm);
	else
		key = hs_find_key(key_code);

	temp_key_code = key_code;

	if (key_parm == HS_REL_K)
		key_code = key_parm;

	//pr_err(">>>> MSM PMIC KEY : %x => %d\n", key_code, key);

	switch (key) {
	case KEY_POWER:
	case KEY_END:
		input_report_key(hi->input, key, (key_code != HS_REL_K));
		break;
	case -1:
		printk(KERN_ERR "%s: No mapping for remote handset event %d\n",
				 __func__, temp_key_code);
		return;
	}
	input_sync(hi->input);
}

void report_s1_hw_key(unsigned int key)
{
	if (hi)
	{
		input_report_key(hi->input, key, 1);
		input_sync(hi->input);

		input_report_key(hi->input, key, 0);
		input_sync(hi->input);
	}
}
EXPORT_SYMBOL(report_s1_hw_key);

static int handle_hs_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len)
{
	struct rpc_keypad_pass_key_code_args {
		uint32_t key_code;
		uint32_t key_parm;
	};

	switch (req->procedure) {
	case RPC_KEYPAD_NULL_PROC:
		return 0;

	case RPC_KEYPAD_PASS_KEY_CODE_PROC: {
		struct rpc_keypad_pass_key_code_args *args;

		args = (struct rpc_keypad_pass_key_code_args *)(req + 1);
		args->key_code = be32_to_cpu(args->key_code);
		args->key_parm = be32_to_cpu(args->key_parm);

		report_hs_key(args->key_code, args->key_parm);

		return 0;
	}

	case RPC_KEYPAD_SET_PWR_KEY_STATE_PROC:
		/* This RPC function must be available for the ARM9
		 * to function properly.  This function is redundant
		 * when RPC_KEYPAD_PASS_KEY_CODE_PROC is handled. So
		 * input_report_key is not needed.
		 */
		return 0;
	default:
		return -ENODEV;
	}
}

static struct msm_rpc_server hs_rpc_server = {
	.prog		= HS_SERVER_PROG,
	.vers		= HS_SERVER_VERS,
	.rpc_call	= handle_hs_rpc_call,
};

static int process_subs_srvc_callback(struct hs_event_cb_recv *recv)
{
	if (!recv)
		return -ENODATA;

	report_hs_key(be32_to_cpu(recv->key.code), be32_to_cpu(recv->key.parm));

	return 0;
}

static void process_hs_rpc_request(uint32_t proc, void *data)
{
	if (proc == HS_EVENT_CB_PROC)
		process_subs_srvc_callback(data);
	else
		pr_err("%s: unknown rpc proc %d\n", __func__, proc);
}

static int hs_rpc_register_subs_arg(struct msm_rpc_client *client,
				    void *buffer, void *data)
{
	struct hs_subs_rpc_req {
		uint32_t hs_subs_ptr;
		struct hs_subs hs_subs;
		uint32_t hs_cb_id;
		uint32_t hs_handle_ptr;
		uint32_t hs_handle_data;
	};

	struct hs_subs_rpc_req *req = buffer;

	req->hs_subs_ptr	= cpu_to_be32(0x1);
	req->hs_subs.ver	= cpu_to_be32(0x1);
	req->hs_subs.srvc	= cpu_to_be32(HS_SUBS_RCV_EVNT);
	req->hs_subs.req	= cpu_to_be32(HS_SUBS_REGISTER);
	req->hs_subs.host_os	= cpu_to_be32(0x4); /* linux */
	req->hs_subs.disc	= cpu_to_be32(HS_SUBS_RCV_EVNT);
	req->hs_subs.id.evnt	= cpu_to_be32(HS_EVNT_CLASS_ALL);

	req->hs_cb_id		= cpu_to_be32(0x1);

	req->hs_handle_ptr	= cpu_to_be32(0x1);
	req->hs_handle_data	= cpu_to_be32(0x0);

	return sizeof(*req);
}

static int hs_rpc_register_subs_res(struct msm_rpc_client *client,
				    void *buffer, void *data)
{
	uint32_t result;

	result = be32_to_cpu(*((uint32_t *)buffer));
	pr_debug("%s: request completed: 0x%x\n", __func__, result);

	return 0;
}

static int hs_cb_func(struct msm_rpc_client *client, void *buffer, int in_size)
{
	int rc = -1;

	struct rpc_request_hdr *hdr = buffer;

	hdr->type = be32_to_cpu(hdr->type);
	hdr->xid = be32_to_cpu(hdr->xid);
	hdr->rpc_vers = be32_to_cpu(hdr->rpc_vers);
	hdr->prog = be32_to_cpu(hdr->prog);
	hdr->vers = be32_to_cpu(hdr->vers);
	hdr->procedure = be32_to_cpu(hdr->procedure);

	process_hs_rpc_request(hdr->procedure,
			    (void *) (hdr + 1));

	msm_rpc_start_accepted_reply(client, hdr->xid,
				     RPC_ACCEPTSTAT_SUCCESS);
	rc = msm_rpc_send_accepted_reply(client, 0);
	if (rc) {
		pr_err("%s: sending reply failed: %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int __init hs_rpc_cb_init(void)
{
	int rc = 0, i, num_vers;

	num_vers = ARRAY_SIZE(rpc_vers);

	for (i = 0; i < num_vers; i++) {
		rpc_client = msm_rpc_register_client("hs",
			HS_RPC_PROG, rpc_vers[i], 0, hs_cb_func);

		if (IS_ERR(rpc_client))
			pr_err("%s: RPC Client version %x failed, fallback\n",
				 __func__, rpc_vers[i]);
		else
			break;
	}

	if (IS_ERR(rpc_client)) {
		pr_err("%s: Incompatible RPC version error %ld\n",
			 __func__, PTR_ERR(rpc_client));
		return PTR_ERR(rpc_client);
	}

	rc = msm_rpc_client_req(rpc_client, HS_SUBSCRIBE_SRVC_PROC,
				hs_rpc_register_subs_arg, NULL,
				hs_rpc_register_subs_res, NULL, -1);
	if (rc) {
		pr_err("%s: RPC client request failed for subscribe services\n",
						__func__);
		goto err_client_req;
	}

	return 0;
err_client_req:
	msm_rpc_unregister_client(rpc_client);
	return rc;
}

static int __devinit hs_rpc_init(void)
{
	int rc;

	rc = hs_rpc_cb_init();
	if (rc) {
		pr_err("%s: failed to initialize rpc client\n", __func__);
		return rc;
	}

	rc = msm_rpc_create_server(&hs_rpc_server);
	if (rc)
		pr_err("%s: failed to create rpc server\n", __func__);

	return rc;
}

static void __devexit hs_rpc_deinit(void)
{
	if (rpc_client)
		msm_rpc_unregister_client(rpc_client);
}


static ssize_t s1_h2w_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(&hi->sdev)) {
	case NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case S1_HEADSET:
		return sprintf(buf, "Headset\n");
	case S1_HEADPHONE:
		return sprintf(buf, "Headphone\n");
	}
	return -EINVAL;
}

static int insert_headset(void)
{
	int		state;
	
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)	
	unsigned	adc;
#endif

	H2W_DBG("");

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
	gpio_set_value(S1_GPIO_MIC_EN, 1);
	mdelay(3);
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	msm_proc_comm(PCOM_OEM_GET_EAR_BUTTON_ADC, &adc, NULL);
	printk(KERN_INFO "EAR ADC : %d\n", adc);
	if (adc < 60)
	{
		state = S1_HEADPHONE;
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
		gpio_set_value(S1_GPIO_MIC_EN, 0);
#endif
	}
	else if (adc < 4095)
	{
		state = S1_HEADSET;
	}
	else
	{
		state = NO_DEVICE;
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
		gpio_set_value(S1_GPIO_MIC_EN, 0);
#endif
	}
#else
	state = S1_HEADSET;
#endif

	switch_set_state(&hi->sdev, state);

	return (state);
}

static void remove_headset(void)
{
	H2W_DBG("");
	switch_set_state(&hi->sdev, NO_DEVICE);
	
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
	gpio_set_value(S1_GPIO_MIC_EN, 0);
#endif	
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
static irqreturn_t button_irq_handler(int irq, void *dev_id);
#endif

static void jack_event_work(struct work_struct *work)
{
	int	state;
	
	H2W_DBG("");
	if (gpio_get_value(S1_GPIO_JACK_SENSE) == 0)
	{
		if (switch_get_state(&hi->sdev) != NO_DEVICE)
		{
			remove_headset();
			if (switch_get_state(&hi->sdev) == S1_HEADSET)
			{
				free_irq(hi->irq_btn, 0);
				set_irq_wake(hi->irq_btn, 0);
			}
		}
	}
	else if (switch_get_state(&hi->sdev) == NO_DEVICE)
	{
		state = insert_headset();
		
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
		if (state == S1_HEADSET)
		{
			int ret;
			
			ret = request_irq(hi->irq_btn, button_irq_handler,
				  IRQF_TRIGGER_FALLING, "h2w_button_detect", NULL);
			pr_debug("%d : request irq : %d\n", hi->irq_btn, ret);
			ret = set_irq_wake(hi->irq_btn, 1);
			pr_debug("set_irq_wake : %d\n", ret);
		}
		else if (state == NO_DEVICE)
		{
			hrtimer_start(&hi->jack_timer, hi->jack_debounce_time, 
				      HRTIMER_MODE_REL);
		}
#endif
	}
}

DECLARE_WORK(jack_work,jack_event_work);

static enum hrtimer_restart jack_event_timer_func(struct hrtimer *data)
{
	schedule_work(&jack_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t jack_detect_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	do {
		value1 = gpio_get_value(S1_GPIO_JACK_SENSE);
		set_irq_type(hi->irq_jack, value1 ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(S1_GPIO_JACK_SENSE);
	} while (value1 != value2 && retry_limit-- > 0);
	hrtimer_start(&hi->jack_timer, hi->jack_debounce_time, HRTIMER_MODE_REL);

	return IRQ_HANDLED;
}

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)

struct adc_range
{
    int min;
    int max;
    int key;
};

static struct adc_range g_tblBtnADC[] = 
{
    { 1560, 2150, KEY_VOLUMEDOWN },
    {  800, 1170, KEY_VOLUMEUP   },
//    {    0,  400, KEY_F22        }  /* mapping to HEADSETHOOK  old */
    {  150,  400, KEY_F22        }  /* mapping to HEADSETHOOK */
};

static void button_event_work(struct work_struct *work)
{
	unsigned    adc;
	int         i, value;
	static int  key;
	static int  pressed = 0;

	H2W_DBG("");
	value = gpio_get_value(S1_GPIO_EAR_KEY_nDET);
	if (value == 0)
	{
		msm_proc_comm(PCOM_OEM_GET_EAR_BUTTON_ADC, &adc, NULL);
		//pr_err("$$$ BUTTON ADC : %d\n", adc);

		key = KEY_RESERVED;
		for (i = 0; i < ARRAY_SIZE(g_tblBtnADC); i++)
		{
			if (g_tblBtnADC[i].min <= adc && adc <= g_tblBtnADC[i].max)
			{
				key = g_tblBtnADC[i].key;
				break;
			}
		}
		
		if (key != KEY_RESERVED)
		{
			//pr_err("$$$ BUTTON ADC : %d, KEY:%d\n", adc, key);
			input_report_key(hi->input, key, 1);
			pressed = 1;
		}
	}
	else if (key != KEY_RESERVED && pressed == 1)
	{
		input_report_key(hi->input, key, 0);
		//pr_err("$$$ BUTTON REL, KEY:%d\n", key);
		pressed = 0;
	}
}

DECLARE_WORK(button_work, button_event_work);

static enum hrtimer_restart button_event_timer_func(struct hrtimer *data)
{
	if (switch_get_state(&hi->sdev) == S1_HEADSET) 
	{
		schedule_work(&button_work);
	}
	return HRTIMER_NORESTART;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	/* check button state only headset is plugged */
	if (switch_get_state(&hi->sdev) == S1_HEADSET)
	{
		do {
			value1 = gpio_get_value(S1_GPIO_EAR_KEY_nDET);
			set_irq_type(hi->irq_btn, value1 ?
					IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
			value2 = gpio_get_value(S1_GPIO_EAR_KEY_nDET);
		} while (value1 != value2 && retry_limit-- > 0);

		hrtimer_start(&hi->btn_timer, hi->btn_debounce_time, HRTIMER_MODE_REL);
	}

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_DEBUG_FS)
static int h2w_debug_set(void *data, u64 val)
{
	switch_set_state(&hi->sdev, (int)val);
	return 0;
}

static int h2w_debug_get(void *data, u64 *val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(h2w_debug_fops, h2w_debug_get, h2w_debug_set, "%llu\n");
static int __init h2w_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("h2w", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("state", 0644, dent, NULL, &h2w_debug_fops);

	return 0;
}

device_initcall(h2w_debug_init);
#endif

static int s1_h2w_probe(struct platform_device *pdev)
{
	int ret;
	u32 cfgJack = GPIO_CFG(S1_GPIO_JACK_SENSE,   0, GPIO_INPUT, 
		               GPIO_NO_PULL, GPIO_2MA);
	
#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)	
	u32 cfgBtn  = GPIO_CFG(S1_GPIO_EAR_KEY_nDET, 0, GPIO_INPUT, 
	                       GPIO_NO_PULL, GPIO_2MA);
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
	u32 cfgMicEn = GPIO_CFG(S1_GPIO_MIC_EN, 0, GPIO_OUTPUT, 
	                        GPIO_NO_PULL, GPIO_2MA); 
#endif

	printk(KERN_INFO "H2W: Registering H2W (headset) driver\n");
	hi = kzalloc(sizeof(struct h2w_info), GFP_KERNEL);
	if (!hi)
		return -ENOMEM;

	hi->sdev.name = "h2w";
	hi->sdev.print_name = s1_h2w_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0)
		return ret;

	ret = hs_rpc_init();
	if (ret)
		goto err_exit;


	/* jack detection set-up*/
	ret = gpio_tlmm_config(cfgJack, GPIO_ENABLE);
	if (ret) {
		pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
		       " failed: %d\n", GPIO_PIN(cfgJack), ret);
		goto err_exit;
	}	

	ret = gpio_request(S1_GPIO_JACK_SENSE, "h2w_switch");
	if (ret < 0)
		goto err_exit;

	ret = gpio_direction_input(S1_GPIO_JACK_SENSE);
	if (ret < 0)
		goto err_exit;

	hi->irq_jack = gpio_to_irq(S1_GPIO_JACK_SENSE);
	if (hi->irq_jack < 0) 
	{
		ret = hi->irq_jack;
		goto err_exit;
	}

	hi->jack_debounce_time = ktime_set(0, 200000000); /* 200 ms */
	hrtimer_init(&hi->jack_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->jack_timer.function = jack_event_timer_func;

	ret = request_irq(hi->irq_jack, jack_detect_irq_handler,
			  IRQF_TRIGGER_HIGH, "h2w_jack_detect", NULL);	
	if (ret < 0)
		goto err_exit;
	
	ret = set_irq_wake(hi->irq_jack, 1);
	if (ret < 0)
		goto err_exit;

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	/* ear-phone keys detection set-up */
	ret = gpio_tlmm_config(cfgBtn, GPIO_ENABLE);
	if (ret) {
		pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
		       " failed: %d\n", GPIO_PIN(cfgJack), ret);
		goto err_exit;
	}
	
	ret = gpio_request(S1_GPIO_EAR_KEY_nDET, "h2w_ear_button");
	if (ret < 0)
		goto err_exit;

	hi->irq_btn = gpio_to_irq(S1_GPIO_EAR_KEY_nDET);
	if (hi->irq_btn < 0) 
	{
		ret = hi->irq_btn;
		goto err_exit;
	}

	hi->btn_debounce_time = ktime_set(0, 20000000); /* 20 ms */
	hrtimer_init(&hi->btn_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hi->btn_timer.function = button_event_timer_func;
#endif

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_ES02)
	ret = gpio_tlmm_config(cfgMicEn, GPIO_ENABLE);
	if (ret) {
		pr_err("gpio_tlmm_config(pin %d, GPIO_ENABLE)"
		       " failed: %d\n", GPIO_PIN(cfgMicEn), ret);
		goto err_exit;
	}
	gpio_set_value(S1_GPIO_MIC_EN, 0);
#endif

	hi->input = input_allocate_device();
	if (!hi->input) {
		ret = -ENOMEM;
		goto err_exit;
	}

	hi->input->name = "s1_headset";
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(hi->input, EV_SW,  SW_HEADPHONE_INSERT);
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(hi->input, EV_KEY, KEY_F22);
	input_set_capability(hi->input, EV_KEY, KEY_END);
	input_set_capability(hi->input, EV_KEY, KEY_POWER);

	ret = input_register_device(hi->input);
	if (ret < 0)
		goto err_exit;

	return 0;

err_exit:
	if (hi->input)
		input_free_device(hi->input);
	if (hi->irq_jack)
		free_irq(hi->irq_jack, 0);
	gpio_free(S1_GPIO_JACK_SENSE);

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)
	if (hi->irq_btn)
		free_irq(hi->irq_btn, 0);
	gpio_free(S1_GPIO_EAR_KEY_nDET);
#endif

	switch_dev_unregister(&hi->sdev);
	printk(KERN_ERR "H2W: Failed to register driver\n");
	
	return ret;
}

static int s1_h2w_remove(struct platform_device *pdev)
{
	H2W_DBG("");
	if (switch_get_state(&hi->sdev))
		remove_headset();

	input_unregister_device(hi->input);
	gpio_free(S1_GPIO_JACK_SENSE);
	free_irq(hi->irq_jack, 0);

#if (CONFIG_S1_BOARD_VER >= S1_BOARD_VER_WS01)	
	gpio_free(S1_GPIO_EAR_KEY_nDET);
	free_irq(hi->irq_btn, 0);
#endif

	switch_dev_unregister(&hi->sdev);

	return 0;
}

static struct platform_device s1_h2w_device = {
	.name		= "s1-h2w",
};

static struct platform_driver s1_h2w_driver = {
	.probe		= s1_h2w_probe,
	.remove		= s1_h2w_remove,
	.driver		= {
		.name		= "s1-h2w",
		.owner		= THIS_MODULE,
	},
};

static int __init s1_h2w_init(void)
{
	int ret;
	
	H2W_DBG("");
	ret = platform_driver_register(&s1_h2w_driver);
	if (ret)
		return ret;
	return platform_device_register(&s1_h2w_device);
}

static void __exit s1_h2w_exit(void)
{
	platform_device_unregister(&s1_h2w_device);
	platform_driver_unregister(&s1_h2w_driver);
}

late_initcall(s1_h2w_init);
module_exit(s1_h2w_exit);

MODULE_AUTHOR("TJ Kim <tjkim@sk-w.com>");
MODULE_DESCRIPTION("S1 Headset & ICC detection driver");
MODULE_LICENSE("GPL");
