#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/sort.h>
#include <linux/platform_device.h>

#include "ir_key_def.h"
#include "sm.h"

static void (*sm_int_save)(void);
static struct input_dev *ir_input;

struct ir_keymap {
	MV_IR_KEY_CODE_t ircode;
	__u32 keycode;
};

#define MAKE_KEYCODE(modifier, scancode) ((modifier<<16)|scancode)

#define MAKE_IR_KEYMAP(i, m, s)		\
	{.ircode = i, .keycode = MAKE_KEYCODE(m, s) }

static inline int galois_ir_get_modifier(struct ir_keymap *keymap)
{
	return (keymap->keycode >> 16) & 0xffff;
}

static inline int galois_ir_get_scancode(struct ir_keymap *keymap)
{
	return keymap->keycode & 0xffff;
}

static int galois_ir_cmp(const void *x1, const void *x2)
{
	const struct ir_keymap *ir1 = (const struct ir_keymap *)x1;
	const struct ir_keymap *ir2 = (const struct ir_keymap *)x2;

	return ir1->ircode - ir2->ircode;
}

static struct ir_keymap *
galois_ir_bsearch(struct ir_keymap *keymap,
		  int nr_keymap,
		  MV_IR_KEY_CODE_t ircode)
{
	int i, j, m;

	i = 0;
	j = nr_keymap - 1;
	while (i <= j) {
		m = (i + j) / 2;
		if (keymap[m].ircode == ircode)
			return &(keymap[m]);
		else if (keymap[m].ircode > ircode)
			j = m - 1;
		else
			i = m + 1;
	}
	return NULL;
}

/*
 * Default IR Key mapping
 */
static struct ir_keymap keymap_tab[] = {
	MAKE_IR_KEYMAP(MV_IR_KEY_NULL,          0, KEY_RESERVED), /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_POWER,	        0, KEY_POWER),
	MAKE_IR_KEYMAP(MV_IR_KEY_OPENCLOSE,     0, KEY_OPEN),

	/* digital keys */
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_1,       0, KEY_1),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_2,       0, KEY_2),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_3,       0, KEY_3),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_4,       0, KEY_4),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_5,       0, KEY_5),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_6,       0, KEY_6),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_7,       0, KEY_7),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_8,       0, KEY_8),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_9,       0, KEY_9),
	MAKE_IR_KEYMAP(MV_IR_KEY_DIGIT_0,       0, KEY_0),

	/* for BD */
	MAKE_IR_KEYMAP(MV_IR_KEY_HOME,          0, KEY_HOME),
	MAKE_IR_KEYMAP(MV_IR_KEY_BACK,          0, KEY_BACK),
	MAKE_IR_KEYMAP(MV_IR_KEY_INFO,          0, KEY_INFO),
	MAKE_IR_KEYMAP(MV_IR_KEY_SETUPMENU,     0, KEY_HOME),
	MAKE_IR_KEYMAP(MV_IR_KEY_CANCEL,        0, KEY_CANCEL), /* no key */

	MAKE_IR_KEYMAP(MV_IR_KEY_DISCMENU,      0, KEY_CONTEXT_MENU),
	MAKE_IR_KEYMAP(MV_IR_KEY_TITLEMENU,     0, KEY_TITLE),
	MAKE_IR_KEYMAP(MV_IR_KEY_SUBTITLE,      0, KEY_SUBTITLE),
	MAKE_IR_KEYMAP(MV_IR_KEY_ANGLE,         0, KEY_ANGLE),
	MAKE_IR_KEYMAP(MV_IR_KEY_AUDIO,         0, KEY_AUDIO),
	MAKE_IR_KEYMAP(MV_IR_KEY_SEARCH,        0, KEY_SEARCH),
	MAKE_IR_KEYMAP(MV_IR_KEY_ZOOM,          0, KEY_ZOOM),
	MAKE_IR_KEYMAP(MV_IR_KEY_DISPLAY,       0, KEY_SCREEN),

	MAKE_IR_KEYMAP(MV_IR_KEY_REPEAT,        0, KEY_MEDIA_REPEAT),
	MAKE_IR_KEYMAP(MV_IR_KEY_REPEAT_AB,     0, KEY_AB),
	MAKE_IR_KEYMAP(MV_IR_KEY_EXIT,          0, KEY_EXIT),
	MAKE_IR_KEYMAP(MV_IR_KEY_A,             0, KEY_RED),
	MAKE_IR_KEYMAP(MV_IR_KEY_B,             0, KEY_GREEN),
	MAKE_IR_KEYMAP(MV_IR_KEY_C,             0, KEY_YELLOW),
	MAKE_IR_KEYMAP(MV_IR_KEY_D,             0, KEY_BLUE),

	/* IR misc around ENTER */
	MAKE_IR_KEYMAP(MV_IR_KEY_CLEAR,         0, KEY_CLEAR),
	MAKE_IR_KEYMAP(MV_IR_KEY_VIDEO_FORMAT,  0, KEY_VIDEO),
	MAKE_IR_KEYMAP(MV_IR_KEY_RETURN,        0, KEY_ESC),

	/* up down left right enter */
	MAKE_IR_KEYMAP(MV_IR_KEY_UP,            0, KEY_UP),
	MAKE_IR_KEYMAP(MV_IR_KEY_DOWN,          0, KEY_DOWN),
	MAKE_IR_KEYMAP(MV_IR_KEY_LEFT,          0, KEY_LEFT),
	MAKE_IR_KEYMAP(MV_IR_KEY_RIGHT,         0, KEY_RIGHT),
	MAKE_IR_KEYMAP(MV_IR_KEY_ENTER,         0, KEY_ENTER),

	/* for BD */
	MAKE_IR_KEYMAP(MV_IR_KEY_SLOW,          0, KEY_SLOW),
	MAKE_IR_KEYMAP(MV_IR_KEY_PAUSE,         0, KEY_PAUSE),
	MAKE_IR_KEYMAP(MV_IR_KEY_PLAY,          0, KEY_PLAY),
	MAKE_IR_KEYMAP(MV_IR_KEY_STOP,          0, KEY_STOP),
	MAKE_IR_KEYMAP(MV_IR_KEY_PLAY_PAUSE,    0, KEY_PLAYPAUSE), /* no key */

	MAKE_IR_KEYMAP(MV_IR_KEY_SKIP_BACKWARD, 0, KEY_PREVIOUS),
	MAKE_IR_KEYMAP(MV_IR_KEY_SKIP_FORWARD,  0, KEY_NEXT),
	MAKE_IR_KEYMAP(MV_IR_KEY_SLOW_BACKWARD, 0, KEY_RESERVED),  /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_SLOW_FORWARD,  0, KEY_RESERVED),  /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_FAST_BACKWARD, 0, KEY_REWIND),
	MAKE_IR_KEYMAP(MV_IR_KEY_FAST_FORWARD,  0, KEY_FORWARD),

	/* bottom keys */
	MAKE_IR_KEYMAP(MV_IR_KEY_F1,            0, KEY_F1),
	MAKE_IR_KEYMAP(MV_IR_KEY_F2,            0, KEY_F2),
	MAKE_IR_KEYMAP(MV_IR_KEY_F3,            0, KEY_F3),
	MAKE_IR_KEYMAP(MV_IR_KEY_F4,            0, KEY_F4),
	MAKE_IR_KEYMAP(MV_IR_KEY_F5,            0, KEY_F5),
	MAKE_IR_KEYMAP(MV_IR_KEY_F6,            0, KEY_F6),
	MAKE_IR_KEYMAP(MV_IR_KEY_F7,            0, KEY_F7),
	MAKE_IR_KEYMAP(MV_IR_KEY_F8,            0, KEY_F8),

	/* for future */
	MAKE_IR_KEYMAP(MV_IR_KEY_VOL_PLUS,      0, KEY_VOLUMEUP),   /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_VOL_MINUS,     0, KEY_VOLUMEDOWN), /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_VOL_MUTE,      0, KEY_MUTE),       /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_CHANNEL_PLUS,  0, KEY_CHANNELUP),  /* no key */
	MAKE_IR_KEYMAP(MV_IR_KEY_CHANNEL_MINUS, 0, KEY_CHANNELDOWN),/* no key */

	/* obsoleted keys */
	MAKE_IR_KEYMAP(MV_IR_KEY_MENU,          0, KEY_MENU),
	MAKE_IR_KEYMAP(MV_IR_KEY_INPUTSEL,      0, KEY_RESERVED),
	MAKE_IR_KEYMAP(MV_IR_KEY_ANYNET,        0, KEY_RESERVED),
	MAKE_IR_KEYMAP(MV_IR_KEY_TELEVISION,    0, KEY_TV),
	MAKE_IR_KEYMAP(MV_IR_KEY_CHANNEL_LIST,  0, KEY_RESERVED),
	MAKE_IR_KEYMAP(MV_IR_KEY_TVPOWER,       0, KEY_RESERVED),
};

static int ir_input_open(struct input_dev *dev)
{
	int msg = MV_SM_IR_Linuxready;

	bsm_msg_send(MV_SM_ID_IR, &msg, 4);
	bsm_msg_send(MV_SM_ID_POWER, &msg, 4);

	return 0;
}

static void ir_report_key(int ir_key)
{
	struct ir_keymap *km;

	km = galois_ir_bsearch(keymap_tab,
			ARRAY_SIZE(keymap_tab), (ir_key & 0xffff));
	if (km) {
		int scancode = galois_ir_get_scancode(km);

		if (ir_key & 0x80000000)
			input_event(ir_input, EV_KEY, scancode, 2);
		else {
			int pressed = !(ir_key & 0x8000000);
			int modifier = galois_ir_get_modifier(km);

			if (pressed && modifier)
				input_event(ir_input, EV_KEY,
						modifier, pressed);

			input_event(ir_input, EV_KEY, scancode, pressed);

			if (!pressed && modifier)
				input_event(ir_input, EV_KEY,
						modifier, pressed);
		}

		input_sync(ir_input);
	} else
		printk(KERN_WARNING "lack of key mapping for 0x%x\n", ir_key);
}

static void girl_sm_int(void)
{
	int msg[8], len, ret, ir_key;

	ret = bsm_msg_recv(MV_SM_ID_IR, msg, &len);

	if(ret != 0 ||  len != 4)
		return;

	ir_key = msg[0];
	ir_report_key(ir_key);
}

static int berlin_ir_probe(struct platform_device *pdev)
{
	int i, error, scancode, modifier;

	ir_input = input_allocate_device();
	if (!ir_input) {
		printk("error: failed to alloc input device for IR.\n");
		return -ENOMEM;
	}

	ir_input->name = "Infra-Red";
	ir_input->phys = "Infra-Red/input0";
	ir_input->id.bustype = BUS_HOST;
	ir_input->id.vendor = 0x0001;
	ir_input->id.product = 0x0001;
	ir_input->id.version = 0x0100;

	ir_input->open = ir_input_open;

	/* sort the keymap in order */
	sort(keymap_tab, ARRAY_SIZE(keymap_tab),
		sizeof(struct ir_keymap), galois_ir_cmp, NULL);

	for (i = 0; i < ARRAY_SIZE(keymap_tab); i++) {
		scancode = galois_ir_get_scancode(&(keymap_tab[i]));
		modifier = galois_ir_get_modifier(&(keymap_tab[i]));
		__set_bit(scancode, ir_input->keybit);
		if (modifier)
			__set_bit(modifier, ir_input->keybit);
	}
	__set_bit(EV_KEY, ir_input->evbit);

	error = input_register_device(ir_input);
	if (error) {
		printk("error: failed to register input device for IR\n");
		input_free_device(ir_input);
		return error;
	}

	sm_int_save = ir_sm_int;
	ir_sm_int = girl_sm_int;
	return 0;
}

static int berlin_ir_remove(struct platform_device *pdev)
{
	input_unregister_device(ir_input);
	input_free_device(ir_input);
	ir_sm_int = sm_int_save;

	return 0;
}

#ifdef CONFIG_PM
static int ir_resume(struct device *dev)
{
	int msg = MV_SM_IR_Linuxready;

	bsm_msg_send(MV_SM_ID_IR, &msg, sizeof(msg));
	bsm_msg_send(MV_SM_ID_POWER, &msg, sizeof(msg));
	return 0;
}

static struct dev_pm_ops ir_pm_ops = {
	.resume		= ir_resume,
};
#endif

static const struct of_device_id berlin_ir_of_match[] = {
	{ .compatible = "marvell,berlin-ir", },
	{},
};
MODULE_DEVICE_TABLE(of, berlin_ir_of_match);

static struct platform_driver berlin_ir_driver = {
	.probe		= berlin_ir_probe,
	.remove		= berlin_ir_remove,
	.driver	= {
		.name	= "berlin-ir",
		.owner  = THIS_MODULE,
		.of_match_table = berlin_ir_of_match,
#ifdef CONFIG_PM
		.pm	= &ir_pm_ops,
#endif
	},
};
module_platform_driver(berlin_ir_driver);

MODULE_AUTHOR("Marvell-Galois");
MODULE_DESCRIPTION("Galois Infra-Red Driver");
MODULE_LICENSE("GPL");
