

#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <mach/board.h>
#include <asm/mach-types.h>

struct vkey_info {
	struct input_dev *input;
};
static struct vkey_info *s1_vkey;


static int s1_vkey_probe(struct platform_device *pdev)
{
	int	ret = 0;

	s1_vkey = kzalloc(sizeof(struct vkey_info), GFP_KERNEL);

	s1_vkey->input = input_allocate_device();
	if (!(s1_vkey->input))
	{
		ret = -ENOMEM;
		goto err_free_mem;
	}

	s1_vkey->input->name = "s1_vkey";
	s1_vkey->input->id.bustype = BUS_VIRTUAL;

	s1_vkey->input->evbit[0] = BIT_MASK(EV_KEY);
	set_bit(KEY_0, s1_vkey->input->keybit);
	set_bit(KEY_1, s1_vkey->input->keybit);
	set_bit(KEY_2, s1_vkey->input->keybit);
	set_bit(KEY_3, s1_vkey->input->keybit);
	set_bit(KEY_4, s1_vkey->input->keybit);
	set_bit(KEY_5, s1_vkey->input->keybit);
	set_bit(KEY_6, s1_vkey->input->keybit);
	set_bit(KEY_7, s1_vkey->input->keybit);
	set_bit(KEY_8, s1_vkey->input->keybit);
	set_bit(KEY_9, s1_vkey->input->keybit);
	set_bit(KEY_SEND, s1_vkey->input->keybit);

	ret = input_register_device(s1_vkey->input);
	if (ret)
		goto err_free_mem;

	return ret;
 
err_free_mem:
	input_free_device(s1_vkey->input);
	return ret;
}

static int s1_vkey_remove(struct platform_device *pdev)
{
	input_unregister_device(s1_vkey->input);

	return 0;
}

static struct platform_device s1_vkey_device = {
	.name		= "s1-vkey",
};

static struct platform_driver s1_vkey_driver = {
	.probe		= s1_vkey_probe,
	.remove		= s1_vkey_remove,
	.driver		= {
		.name		= "s1-vkey",
		.owner		= THIS_MODULE,
	},
};

static int __init s1_vkey_init(void)
{
	int ret;
	
	ret = platform_driver_register(&s1_vkey_driver);
	if (ret)
		return ret;
	return platform_device_register(&s1_vkey_device);
}

static void __exit s1_vkey_exit(void)
{
	platform_device_unregister(&s1_vkey_device);
	platform_driver_unregister(&s1_vkey_driver);
}

module_init(s1_vkey_init);
module_exit(s1_vkey_exit);

MODULE_AUTHOR("Jeongpil Mok <jpmoks@sk-w.com>");
MODULE_DESCRIPTION("S1 virtual keypad driver");
MODULE_LICENSE("GPL");
