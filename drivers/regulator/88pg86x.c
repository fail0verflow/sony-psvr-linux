/*
 *  Author:	Jisheng Zhang <jszhang@marvell.com>
 *  Copyright (c) 2013 Marvell Technology Group Ltd.
 *  		http://www.marvell.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define PG86x_NUM_REGS	2

struct pg86x_priv {
	struct i2c_client *i2c;
	u8 reg[PG86x_NUM_REGS];
	u8 val25[PG86x_NUM_REGS];
	u8 val50[PG86x_NUM_REGS];
	int vol25[PG86x_NUM_REGS];
	u8 validBits[PG86x_NUM_REGS];
	struct regulator_dev *rdev[PG86x_NUM_REGS];
	struct regulator_init_data *reg_data[PG86x_NUM_REGS];
	struct regulator_desc desc[PG86x_NUM_REGS];
};

static int pg86x_is_enabled(struct regulator_dev *rdev)
{
	return 1;
}

static int pg86x_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int id = rdev_get_id(rdev);
	struct pg86x_priv *priv = rdev_get_drvdata(rdev);
	unsigned steps = priv->val50[id] - priv->val25[id];

	if (selector >= priv->desc[id].n_voltages)
		return -EINVAL;
	if (selector >= steps)
		return 1600000 + 50000 * (selector - steps);
	return priv->vol25[id] + 25000 * selector;
}

static int pg86x_get_voltage(struct regulator_dev *rdev)
{
	int ret, id = rdev_get_id(rdev);
	struct pg86x_priv *priv = rdev_get_drvdata(rdev);

	ret = i2c_smbus_read_byte_data(priv->i2c, priv->reg[id]);
	if (ret < 0) {
		dev_err(&priv->i2c->dev, "I2C read error\n");
		return ret;
	}
	ret &= priv->validBits[id];
	if (ret > priv->val50[id] + (2200000 - 1600000) / 50000)
		return -EINVAL;
	if (ret >= priv->val50[id])
		return 1600000 + 50000 * (ret - priv->val50[id]);
	if (ret >= priv->val25[id])
		return priv->vol25[id] + 25000 * (ret - priv->val25[id]);

	return -EINVAL;
}

static int pg86x_set_voltage(struct regulator_dev *rdev, int min_uV,
			     int max_uV, unsigned *selector)
{
	u8 originalValue;
	u8 val;
	int ret, delta, id = rdev_get_id(rdev);
	struct pg86x_priv *priv = rdev_get_drvdata(rdev);

	if (max_uV > 2200000 || max_uV < priv->vol25[id])
		return -EINVAL;
	if (min_uV > 2200000 || min_uV < priv->vol25[id])
		return -EINVAL;
	else if (min_uV >= 1600000) {
		delta = min_uV - 1600000;
		delta = (delta + 50000 - 1 ) / 50000 * 50000;
		delta /= 50000;
		*selector = delta;
		val = delta + priv->val50[id];
	} else {
		delta = min_uV - priv->vol25[id];
		delta = (delta + 25000 - 1 ) / 25000 * 25000;
		delta /= 25000;
		*selector = delta;
		val = delta + priv->val25[id];
	}

	ret = i2c_smbus_read_byte_data(priv->i2c, priv->reg[id]);
	if (ret < 0) {
		dev_err(&priv->i2c->dev, "I2C read error\n");
		return ret;
	}

	originalValue = (u8)ret;
	val = (originalValue & (~(priv->validBits[id]))) +
		(val & (priv->validBits[id]));

	ret = i2c_smbus_write_byte_data(priv->i2c, priv->reg[id], val);
	if (ret < 0)
		dev_err(&priv->i2c->dev, "I2C write error\n");
	return 0;
}

static struct regulator_ops pg86x_regulator_ops = {
	.is_enabled = pg86x_is_enabled,
	.list_voltage = pg86x_list_voltage,
	.set_voltage = pg86x_set_voltage,
	.get_voltage = pg86x_get_voltage,
};

static const struct i2c_device_id pg86x_i2c_id[] = {
	{ "pg86x", 0x867 },
	{},
};
MODULE_DEVICE_TABLE(i2c, pg86x_i2c_id);

static const struct of_device_id pg86x_dt_ids[] = {
	{ .compatible = "marvell,pg86x", },
	{},
};
MODULE_DEVICE_TABLE(of, pg86x_dt_ids);

static struct of_regulator_match pg86x_matches[] = {
	{ .name = "BK1_TV", },
	{ .name = "BK2_TV", },
};

static int pg86x_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int i, ret;
	struct pg86x_priv *priv;
	struct regulator_config config = { };
	struct device_node *np = i2c->dev.of_node;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ret = of_regulator_match(&i2c->dev, np, pg86x_matches, PG86x_NUM_REGS);
	if (ret < 0) {
		dev_err(&i2c->dev, "Error parsing regulator init data: %d\n", ret);
		return ret;
	}

	priv->i2c = i2c;
	i2c_set_clientdata(i2c, priv);

	for (i = 0; i < PG86x_NUM_REGS; i++) {
		struct regulator_dev *rdev;

		if (!pg86x_matches[i].init_data ||
				!pg86x_matches[i].of_node) {
			continue;
		}

		priv->reg_data[i] = pg86x_matches[i].init_data;
		priv->desc[i].name = pg86x_matches[i].name;
		priv->desc[i].id = i;
		priv->desc[i].ops = &pg86x_regulator_ops;
		priv->desc[i].type = REGULATOR_VOLTAGE;
		priv->desc[i].owner = THIS_MODULE;
		priv->desc[i].n_voltages = (i == 0)? 46 : 37;
		priv->reg[i] = (i == 0)? 0x24 : 0x13;
		priv->val25[i] = (i == 0)? 0x2 : 0x10;
		priv->val50[i] = (i == 0)? 0x23 : 0x28;
		priv->vol25[i] = (i == 0)? 775000 : 1000000;
		priv->validBits[i] = (i == 0) ? 0x7F : 0x3F;
		config.dev = &i2c->dev;
		config.init_data = priv->reg_data[i];
		config.driver_data = priv;
		config.of_node = pg86x_matches[i].of_node;
		rdev = regulator_register(&priv->desc[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&i2c->dev, "failed to register pg86x\n");
			ret = PTR_ERR(rdev);

			while (--i >= 0)
				regulator_unregister(priv->rdev[i]);

			return ret;
		}
		priv->rdev[i] = rdev;
	}

	return 0;
}

static int pg86x_remove(struct i2c_client *i2c)
{
	struct pg86x_priv *priv = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < PG86x_NUM_REGS; i++)
		regulator_unregister(priv->rdev[i]);

	return 0;
}

static struct i2c_driver pg86x_driver = {
	.id_table = pg86x_i2c_id,
	.driver = {
		.name = "pg86x",
		.owner = THIS_MODULE,
		.of_match_table = pg86x_dt_ids,
	},
	.probe = pg86x_probe,
	.remove = pg86x_remove,
};

module_i2c_driver(pg86x_driver);

MODULE_DESCRIPTION("Driver for Marvell 88PG86x PMIC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@marvell.com>");
MODULE_LICENSE("GPL v2");
