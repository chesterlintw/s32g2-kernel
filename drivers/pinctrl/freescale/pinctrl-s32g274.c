// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * NXP S32G274 pinctrl driver
 *
 * Copyright 2015-2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2018,2020-2021 NXP
 */

#include <dt-bindings/pinctrl/pinctrl-s32g2.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-s32.h"

/* Pad names for the pinmux subsystem */
static const struct pinctrl_pin_desc s32g2_pinctrl0_pads[] = {
	/* GMAC0 */
	S32_PINCTRL_PIN(S32G2_GMAC0_MDC),
	S32_PINCTRL_PIN(S32G2_GMAC0_MDIO_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXCLK_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXEN_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXD0_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXD1_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXD2_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXD3_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXCLK_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXDV_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD0_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD1_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD2_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD3_OUT),
	S32_PINCTRL_PIN(S32G2_GMAC0_MDIO_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD0_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD1_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD2_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXD3_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXCLK_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_RXDV_IN),
	S32_PINCTRL_PIN(S32G2_GMAC0_TXCLK_IN),
};

static struct s32_pinctrl_soc_info s32g2_pinctrl0_info = {
	.pins = s32g2_pinctrl0_pads,
	.npins = ARRAY_SIZE(s32g2_pinctrl0_pads),
};

static const struct of_device_id s32g2_pinctrl_of_match[] = {
	{
		.compatible = "nxp,s32g2-siul2-pinctrl0",
		.data = (void *) &s32g2_pinctrl0_info,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s32g2_pinctrl_of_match);

static int s32g274_pinctrl_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(s32g2_pinctrl_of_match, &pdev->dev);

	if (!of_id)
		return -ENODEV;

	return s32_pinctrl_probe
			(pdev, (struct s32_pinctrl_soc_info *) of_id->data);
}

static const struct dev_pm_ops s32g274_pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(s32_pinctrl_suspend,
				     s32_pinctrl_resume)
};

static struct platform_driver s32g274_pinctrl_driver = {
	.driver = {
		.name = "s32g274-siul2-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = s32g2_pinctrl_of_match,
		.pm = &s32g274_pinctrl_pm_ops,
	},
	.probe = s32g274_pinctrl_probe,
	.remove = s32_pinctrl_remove,
};

module_platform_driver(s32g274_pinctrl_driver);

MODULE_AUTHOR("Matthew Nunez <matthew.nunez@nxp.com>");
MODULE_DESCRIPTION("NXP S32G274 pinctrl driver");
MODULE_LICENSE("GPL v2");
