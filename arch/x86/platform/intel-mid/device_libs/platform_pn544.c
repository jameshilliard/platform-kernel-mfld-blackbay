/*
 * platform_pn544.c: pn544 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/nfc/pn544.h>
#include <asm/intel-mid.h>
#include "platform_pn544.h"

/* MFLD NFC controller (PN544) platform init */

static int nfc_gpio_enable = -1;
static int nfc_gpio_fw_reset = -1;
static int nfc_gpio_irq = -1;

static int pn544_request_resources(struct i2c_client *client)
{
	int ret;

	ret = gpio_request_one(nfc_gpio_enable, GPIOF_OUT_INIT_LOW,
			       "NFC enable");
	if (ret < 0)
		return ret;

	ret = gpio_request_one(nfc_gpio_fw_reset, GPIOF_OUT_INIT_LOW,
			       "NFC FW reset");
	if (ret < 0)
		goto fail_free_enable;

	ret = gpio_request_one(nfc_gpio_irq, GPIOF_IN, "NFC interrupt\n");
	if (ret < 0)
		goto fail_free_reset;

	client->irq = gpio_to_irq(nfc_gpio_irq);
	return 0;

fail_free_reset:
	gpio_free(nfc_gpio_fw_reset);
fail_free_enable:
	gpio_free(nfc_gpio_enable);

	return ret;
}

static void pn544_free_resources(void)
{
	gpio_free(nfc_gpio_irq);
	gpio_free(nfc_gpio_fw_reset);
	gpio_free(nfc_gpio_enable);
}

static void pn544_enable(int fw)
{
	gpio_set_value(nfc_gpio_enable, 1);
	gpio_set_value(nfc_gpio_fw_reset, !!fw);
}

static void pn544_disable(void)
{
	gpio_set_value(nfc_gpio_enable, 0);
}

static int pn544_get_gpio(int type)
{
	int gpio = -1;

	switch (type) {
	case NFC_GPIO_ENABLE:
		gpio = nfc_gpio_enable;
		break;
	case NFC_GPIO_FW_RESET:
		gpio = nfc_gpio_fw_reset;
		break;
	case NFC_GPIO_IRQ:
		gpio = nfc_gpio_irq;
		break;
	}

	return gpio;
}

static struct regulator_consumer_supply pn544_regs_supply[] = {
	REGULATOR_SUPPLY("Vdd_IO", "2-0028"),
	REGULATOR_SUPPLY("VBat", "2-0028"),
	REGULATOR_SUPPLY("VSim", "2-0028"),
};

static struct regulator_init_data pn544_regs_init_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(pn544_regs_supply),
	.consumer_supplies = pn544_regs_supply,
};

static struct fixed_voltage_config pn544_regs_config = {
	.supply_name		= "pn544_regs",
	.microvolts		= 1800000,
	.gpio			= -EINVAL,
	.init_data		= &pn544_regs_init_data,
};

static struct platform_device pn544_regs = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev = {
		.platform_data	= &pn544_regs_config,
	},
};

void *pn544_platform_data(void *info)
{
	static struct pn544_nfc_platform_data pdata = {
		.request_resources	= pn544_request_resources,
		.free_resources		= pn544_free_resources,
		.enable			= pn544_enable,
		.disable		= pn544_disable,
		.get_gpio		= pn544_get_gpio,
	};

	nfc_gpio_enable = get_gpio_by_name("NFC-enable");
	if (nfc_gpio_enable < 0) {
		pr_err("failed to get NFC enable GPIO\n");
		return NULL;
	}

	nfc_gpio_fw_reset = get_gpio_by_name("NFC-reset");
	if (nfc_gpio_fw_reset < 0) {
		pr_err("failed to get NFC reset GPIO\n");
		return NULL;
	}

	nfc_gpio_irq = get_gpio_by_name("NFC-intr");
	if (nfc_gpio_irq < 0) {
		pr_err("failed to get NFC interrupt GPIO\n");
		return NULL;
	}

	if (platform_device_register(&pn544_regs)) {
		pr_err("failed to register NFC fixed voltage regulator\n");
		return NULL;
	}

	return &pdata;
}
