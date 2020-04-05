#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#define BIT_SM_CTRL_SOC2SM_SW_INTR	(1<<2) /* SoC to SM Software interrupt */
#define BIT_SM_CTRL_ADC_SEL0		(1<<5) /* adc input channel select */
#define BIT_SM_CTRL_ADC_SEL1		(1<<6)
#define BIT_SM_CTRL_ADC_SEL2		(1<<7)
#define BIT_SM_CTRL_ADC_SEL3		(1<<8)
#define BIT_SM_CTRL_ADC_PU		(1<<9)	/* power up */
#define BIT_SM_CTRL_ADC_CKSEL0		(1<<10) /* clock divider select */
#define BIT_SM_CTRL_ADC_CKSEL1		(1<<11)
#define BIT_SM_CTRL_ADC_START		(1<<12) /* start digitalization process */
#define BIT_SM_CTRL_ADC_RESET		(1<<13) /* reset afer power up */
#define BIT_SM_CTRL_ADC_BG_RDY		(1<<14)	/* bandgap reference block ready */
#define BIT_SM_CTRL_ADC_CONT		(1<<15) /* continuous v.s single shot */
#define BIT_SM_CTRL_ADC_BUF_EN		(1<<16) /* enable anolog input buffer */
#define BIT_SM_CTRL_ADC_VREF_SEL	(1<<17) /* ext. v.s. int. ref select */
#define BIT_SM_CTRL_TSEN_EN		(1<<20)
#define BIT_SM_CTRL_TSEN_CLK_SEL	(1<<21)
#define BIT_SM_CTRL_TSEN_MODE_SEL	(1<<22)

#define TSEN_WAIT_MAX			50000
#define NR_ATTRS			2

#define TSEN_VARIANT_BERLIN      1
#define TSEN_VARIANT_BERLIN2CDP  2

struct tsen_adc33_data {
	void __iomem *sm_adc_ctrl;
	void __iomem *sm_adc_status;
	void __iomem *tsen_adc_ctrl;
	void __iomem *tsen_adc_status;
	void __iomem *tsen_adc_data;
	int variant;
	struct mutex lock;
	struct device *hwmon_dev;
	struct sensor_device_attribute *attrs[NR_ATTRS];
};

static int tsen_raw_read(struct tsen_adc33_data *data)
{
	int raw, wait = 0;
	unsigned int status, val;

	mutex_lock(&data->lock);
	val = readl_relaxed(data->sm_adc_ctrl);;
	val &= ~(1 << 29);
	writel_relaxed(val, data->sm_adc_ctrl);

	val = readl_relaxed(data->tsen_adc_ctrl);
	val &= ~(0xf << 21);
	val |= (8 << 22);
	val |= (1 << 21);
	writel_relaxed(val, data->tsen_adc_ctrl);

	val = readl_relaxed(data->sm_adc_ctrl);;
	val |= (1 << 19);
	writel_relaxed(val, data->sm_adc_ctrl);

	val = readl_relaxed(data->tsen_adc_ctrl);
	val |= (1 << 8);
	writel_relaxed(val, data->tsen_adc_ctrl);
	while (1) {
		status = readl_relaxed(data->tsen_adc_status);
		if (status & 0x1)
			break;
		if (++wait > TSEN_WAIT_MAX) {
			mutex_unlock(&data->lock);
			printk(KERN_WARNING "%s timeout, status 0x%08x.\n",
					__func__, status);
			return -ETIMEDOUT;
		}
	}

	raw = readl_relaxed(data->tsen_adc_data);
	status &= ~0x1;
	writel_relaxed(status, data->tsen_adc_status);

	val = readl_relaxed(data->tsen_adc_ctrl);
	val &= ~(1 << 8);
	writel_relaxed(val, data->tsen_adc_ctrl);
	mutex_unlock(&data->lock);

	return raw;
}

static inline int tsen_celcius_calc(int raw, int variant)
{
	if (raw > 2047)
		raw = -(4096 - raw);
	if (variant == TSEN_VARIANT_BERLIN2CDP)
		raw = (raw * 3904 - 2818000) / 10000;
	else
		raw = (raw * 100) / 264 - 270;

	return raw;
}

static ssize_t tsen_temp_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	static int raw;
	int temp;
	struct tsen_adc33_data *data = dev_get_drvdata(dev);

	raw = tsen_raw_read(data);
	if (raw < 0)
		return raw;

	temp = tsen_celcius_calc(raw, data->variant);
	return sprintf(buf, "%u\n", temp);
}

static ssize_t tsen_temp_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int temp;
	struct tsen_adc33_data *data = dev_get_drvdata(dev);

	temp = tsen_raw_read(data);
	if (temp < 0)
		return temp;

	return sprintf(buf, "%u\n", temp);
}

static void tsen_reset(struct tsen_adc33_data *data)
{
	unsigned int val;

	val = readl_relaxed(data->sm_adc_ctrl);;
	val |= BIT_SM_CTRL_SOC2SM_SW_INTR;
	val |= BIT_SM_CTRL_ADC_PU;
	val |= BIT_SM_CTRL_ADC_SEL0;
	val |= BIT_SM_CTRL_ADC_SEL1;
	val |= BIT_SM_CTRL_ADC_SEL2;
	val &= ~BIT_SM_CTRL_ADC_SEL3;
	val &= ~BIT_SM_CTRL_ADC_CKSEL0;
	val &= ~BIT_SM_CTRL_ADC_CKSEL1;
	val &= ~BIT_SM_CTRL_ADC_START;
	val &= ~BIT_SM_CTRL_ADC_RESET;
	writel_relaxed(val, data->sm_adc_ctrl);
	writel_relaxed(0, data->sm_adc_status);
	val |= BIT_SM_CTRL_ADC_RESET;
	writel_relaxed(val, data->sm_adc_ctrl);
	val &= ~BIT_SM_CTRL_ADC_RESET;
	writel_relaxed(val, data->sm_adc_ctrl);
}

static struct sensor_device_attribute tsen_sensor_attrs[] = {
	SENSOR_ATTR(tsen_temp, S_IRUGO, tsen_temp_show, NULL, 0),
	SENSOR_ATTR(tsen_temp_raw, S_IRUGO, tsen_temp_raw_show, NULL, 1),
};

static int tsen_create_attrs(struct device *dev,
				struct sensor_device_attribute *attrs[])
{
	int i;
	int ret;

	for (i = 0; i < NR_ATTRS; i++) {
		attrs[i] = &tsen_sensor_attrs[i];
		sysfs_attr_init(&attrs[i]->dev_attr.attr);
		ret =  device_create_file(dev, &attrs[i]->dev_attr);
		if (ret)
			goto exit_free;
	}
	return 0;

exit_free:
	while (--i >= 0)
		device_remove_file(dev, &attrs[i]->dev_attr);
	return ret;
}

static void tsen_remove_attrs(struct device *dev,
				struct sensor_device_attribute *attrs[])
{
	int i;
	for (i = 0; i < NR_ATTRS; i++) {
		if (attrs[i] == NULL)
			continue;
		device_remove_file(dev, &attrs[i]->dev_attr);
	}
}

static struct of_device_id tsen_adc33_match[] = {
	{
		.compatible = "marvell,berlin-tsen-adc33",
		.data = (void *)TSEN_VARIANT_BERLIN,
	},
	{
		.compatible = "marvell,berlin2cdp-tsen-adc33",
		.data = (void *)TSEN_VARIANT_BERLIN2CDP,
	},
	{},
};
MODULE_DEVICE_TABLE(of, tsen_adc33_match);

static int tsen_adc33_probe(struct platform_device *pdev)
{
	int ret;
	struct tsen_adc33_data *data;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id;

	data = devm_kzalloc(&pdev->dev, sizeof(struct tsen_adc33_data),
			GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Failed to allocate driver structure\n");
		return -ENOMEM;
	}

	of_id = of_match_device(tsen_adc33_match, &pdev->dev);
	if (of_id)
		data->variant = (int)of_id->data;
	else
		data->variant = TSEN_VARIANT_BERLIN;

	data->sm_adc_ctrl = of_iomap(np, 0);
	data->sm_adc_status = of_iomap(np, 1);
	data->tsen_adc_ctrl = of_iomap(np, 2);
	data->tsen_adc_status = of_iomap(np, 3);
	data->tsen_adc_data = of_iomap(np, 4);
	if (!data->sm_adc_ctrl || !data->sm_adc_status ||
		!data->tsen_adc_ctrl || !data->tsen_adc_status ||
		!data->tsen_adc_data) {
		dev_err(&pdev->dev, "Failed to iomap memory\n");
		return -ENOMEM;
	}

	ret = tsen_create_attrs(&pdev->dev, data->attrs);
	if (ret)
		goto err_free;

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);
	tsen_reset(data);

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		dev_err(&pdev->dev, "Failed to register hwmon device\n");
		goto err_create_group;
	}
	return 0;

err_create_group:
	tsen_remove_attrs(&pdev->dev, data->attrs);
err_free:
	iounmap(data->sm_adc_ctrl);
	iounmap(data->sm_adc_status);
	iounmap(data->tsen_adc_ctrl);
	iounmap(data->tsen_adc_status);
	iounmap(data->tsen_adc_data);

	return ret;
}

static int tsen_adc33_remove(struct platform_device *pdev)
{
	struct tsen_adc33_data *data = platform_get_drvdata(pdev);

	tsen_remove_attrs(&pdev->dev, data->attrs);
	hwmon_device_unregister(data->hwmon_dev);
	iounmap(data->sm_adc_ctrl);
	iounmap(data->sm_adc_status);
	iounmap(data->tsen_adc_ctrl);
	iounmap(data->tsen_adc_status);
	iounmap(data->tsen_adc_data);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver tsen_adc33_driver = {
	.driver = {
		.name   = "tsen-adc33",
		.owner  = THIS_MODULE,
		.of_match_table = tsen_adc33_match,
	},
	.probe = tsen_adc33_probe,
	.remove	= tsen_adc33_remove,
};

module_platform_driver(tsen_adc33_driver);

MODULE_DESCRIPTION("TSEN ADC33 Driver");
MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_LICENSE("GPL");
