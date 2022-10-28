// SPDX-License-Identifier: GPL-2.0-only
/*
 * DWMAC Specific Glue layer for NXP S32 Common Chassis
 *
 * Copyright 2019-2022 NXP
 * Copyright (C) 2022 SUSE LLC
 *
 */

#include <linux/device.h>
#include <linux/ethtool.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/stmmac.h>

#include "stmmac_platform.h"

#define GMAC_TX_RATE_125M	125000000	/* 125MHz */
#define GMAC_TX_RATE_25M	25000000	/* 25MHz */
#define GMAC_TX_RATE_2M5	2500000		/* 2.5MHz */

/* S32 SRC register for phyif selection */
#define PHY_INTF_SEL_MII        0x00
#define PHY_INTF_SEL_SGMII      0x01
#define PHY_INTF_SEL_RGMII      0x02
#define PHY_INTF_SEL_RMII       0x08

/* AXI4 ACE control settings */
#define ACE_DOMAIN_SIGNAL	0x2
#define ACE_CACHE_SIGNAL	0xf
#define ACE_CONTROL_SIGNALS	((ACE_DOMAIN_SIGNAL << 4) | ACE_CACHE_SIGNAL)
#define ACE_PROTECTION		0x2

struct s32cc_priv_data {
	void __iomem *ctrl_sts;
	struct device *dev;
	phy_interface_t intf_mode;
	struct clk *tx_clk;
	struct clk *rx_clk;
};

static int s32cc_gmac_init(struct platform_device *pdev, void *priv)
{
	struct s32cc_priv_data *gmac = priv;
	u32 intf_sel;
	int ret;

	ret = clk_prepare_enable(gmac->tx_clk);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable tx clock\n");
		return ret;
	}

	ret = clk_prepare_enable(gmac->rx_clk);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable rx clock\n");
		return ret;
	}

	/* set interface mode */
	switch (gmac->intf_mode) {
	case PHY_INTERFACE_MODE_SGMII:
		intf_sel = PHY_INTF_SEL_SGMII;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
		intf_sel = PHY_INTF_SEL_RGMII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_sel = PHY_INTF_SEL_RMII;
		break;
	case PHY_INTERFACE_MODE_MII:
		intf_sel = PHY_INTF_SEL_MII;
		break;
	default:
		dev_err(&pdev->dev, "Unsupported PHY interface: %s\n",
			phy_modes(gmac->intf_mode));
		return -EINVAL;
	}

	writel(intf_sel, gmac->ctrl_sts);

	dev_dbg(&pdev->dev, "PHY mode set to %s\n", phy_modes(gmac->intf_mode));

	return 0;
}

static void s32cc_gmac_exit(struct platform_device *pdev, void *priv)
{
	struct s32cc_priv_data *gmac = priv;

	clk_disable_unprepare(gmac->tx_clk);
	clk_disable_unprepare(gmac->rx_clk);
}

static void s32cc_fix_speed(void *priv, unsigned int speed)
{
	struct s32cc_priv_data *gmac = priv;

	/* SGMII mode doesn't support the clock reconfiguration */
	if (gmac->intf_mode == PHY_INTERFACE_MODE_SGMII)
		return;

	switch (speed) {
	case SPEED_1000:
		dev_info(gmac->dev, "Set TX clock to 125M\n");
		clk_set_rate(gmac->tx_clk, GMAC_TX_RATE_125M);
		break;
	case SPEED_100:
		dev_info(gmac->dev, "Set TX clock to 25M\n");
		clk_set_rate(gmac->tx_clk, GMAC_TX_RATE_25M);
		break;
	case SPEED_10:
		dev_info(gmac->dev, "Set TX clock to 2.5M\n");
		clk_set_rate(gmac->tx_clk, GMAC_TX_RATE_2M5);
		break;
	default:
		dev_err(gmac->dev, "Unsupported/Invalid speed: %d\n", speed);
		return;
	}
}

static int s32cc_config_cache_coherency(struct platform_device *pdev,
					struct plat_stmmacenet_data *plat_dat)
{
	plat_dat->axi4_ace_ctrl =
		devm_kzalloc(&pdev->dev,
			     sizeof(struct stmmac_axi4_ace_ctrl),
			     GFP_KERNEL);

	if (!plat_dat->axi4_ace_ctrl)
		return -ENOMEM;

	plat_dat->axi4_ace_ctrl->tx_ar_reg = (ACE_CONTROL_SIGNALS << 16)
		| (ACE_CONTROL_SIGNALS << 8) | ACE_CONTROL_SIGNALS;

	plat_dat->axi4_ace_ctrl->rx_aw_reg = (ACE_CONTROL_SIGNALS << 24)
		| (ACE_CONTROL_SIGNALS << 16) | (ACE_CONTROL_SIGNALS << 8)
		| ACE_CONTROL_SIGNALS;

	plat_dat->axi4_ace_ctrl->txrx_awar_reg = (ACE_PROTECTION << 20)
		| (ACE_PROTECTION << 16) | (ACE_CONTROL_SIGNALS << 8)
		| ACE_CONTROL_SIGNALS;

	return 0;
}

static int s32cc_dwmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct s32cc_priv_data *gmac;
	struct resource *res;
	const char *tx_clk, *rx_clk;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	gmac = devm_kzalloc(&pdev->dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac)
		return PTR_ERR(gmac);

	gmac->dev = &pdev->dev;

	/* S32G control reg */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	gmac->ctrl_sts = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(gmac->ctrl_sts)) {
		dev_err(&pdev->dev, "S32CC config region is missing\n");
		return PTR_ERR(gmac->ctrl_sts);
	}

	plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	plat_dat->bsp_priv = gmac;

	switch (plat_dat->phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		tx_clk = "tx_pcs";
		rx_clk = "rx_pcs";
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
		tx_clk = "tx_rgmii";
		rx_clk = "rx_rgmii";
		break;
	case PHY_INTERFACE_MODE_RMII:
		tx_clk = "tx_rmii";
		rx_clk = "rx_rmii";
		break;
	case PHY_INTERFACE_MODE_MII:
		tx_clk = "tx_mii";
		rx_clk = "rx_mii";
		break;
	default:
		dev_err(&pdev->dev, "Not supported phy interface mode: [%s]\n",
			phy_modes(plat_dat->phy_interface));
		return -EINVAL;
	};

	gmac->intf_mode = plat_dat->phy_interface;

	/* DMA cache coherency settings */
	if (of_dma_is_coherent(pdev->dev.of_node)) {
		ret = s32cc_config_cache_coherency(pdev, plat_dat);
		if (ret)
			goto err_remove_config_dt;
	}

	/* tx clock */
	gmac->tx_clk = devm_clk_get(&pdev->dev, tx_clk);
	if (IS_ERR(gmac->tx_clk)) {
		dev_err(&pdev->dev, "Get TX clock failed\n");
		ret = PTR_ERR(gmac->tx_clk);
		goto err_remove_config_dt;
	}

	/* rx clock */
	gmac->rx_clk = devm_clk_get(&pdev->dev, rx_clk);
	if (IS_ERR(gmac->rx_clk)) {
		dev_err(&pdev->dev, "Get RX clock failed\n");
		ret = PTR_ERR(gmac->rx_clk);
		goto err_remove_config_dt;
	}

	ret = s32cc_gmac_init(pdev, gmac);
	if (ret)
		goto err_remove_config_dt;

	/* core feature set */
	plat_dat->has_gmac4 = true;
	plat_dat->pmt = 1;

	plat_dat->init = s32cc_gmac_init;
	plat_dat->exit = s32cc_gmac_exit;
	plat_dat->fix_mac_speed = s32cc_fix_speed;

	/* safety feature config */
	plat_dat->safety_feat_cfg =
		devm_kzalloc(&pdev->dev, sizeof(*plat_dat->safety_feat_cfg),
			     GFP_KERNEL);

	if (!plat_dat->safety_feat_cfg) {
		dev_err(&pdev->dev, "Allocate safety_feat_cfg failed\n");
		goto err_gmac_exit;
	}

	plat_dat->safety_feat_cfg->tsoee = 1;
	plat_dat->safety_feat_cfg->mrxpee = 1;
	plat_dat->safety_feat_cfg->mestee = 1;
	plat_dat->safety_feat_cfg->mrxee = 1;
	plat_dat->safety_feat_cfg->mtxee = 1;
	plat_dat->safety_feat_cfg->epsi = 1;
	plat_dat->safety_feat_cfg->edpp = 1;
	plat_dat->safety_feat_cfg->prtyen = 1;
	plat_dat->safety_feat_cfg->tmouten = 1;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_gmac_exit;

	return 0;

err_gmac_exit:
	s32cc_gmac_exit(pdev, plat_dat->bsp_priv);
err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);
	return ret;
}

static const struct of_device_id s32_dwmac_match[] = {
	{ .compatible = "nxp,s32cc-dwmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, s32_dwmac_match);

static struct platform_driver s32_dwmac_driver = {
	.probe  = s32cc_dwmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "s32cc-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = s32_dwmac_match,
	},
};
module_platform_driver(s32_dwmac_driver);

MODULE_AUTHOR("Jan Petrous <jan.petrous@nxp.com>");
MODULE_DESCRIPTION("NXP S32 common chassis GMAC driver");
MODULE_LICENSE("GPL v2");
