// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * cs35l45.c - CS35L45 ALSA SoC audio driver
 *
 * Copyright 2019 Cirrus Logic, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */

#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "wm_adsp.h"
#include "cs35l45.h"
#include <sound/cs35l45.h>

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
#include <sound/cirrus/core.h>
#include <sound/cirrus/big_data.h>
#include <sound/cirrus/calibration.h>
#include <sound/cirrus/power.h>
#endif

struct cs35l45_mixer_cache {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

static int __cs35l45_initialize(struct cs35l45_private *cs35l45);
static int cs35l45_hibernate(struct cs35l45_private *cs35l45, bool hiber_en);
static int cs35l45_set_sysclk(struct cs35l45_private *cs35l45, int clk_id,
			      unsigned int freq);

static bool cs35l45_is_csplmboxsts_correct(enum cspl_mboxcmd cmd,
					   enum cspl_mboxstate sts)
{
	switch (cmd) {
	case CSPL_MBOX_CMD_NONE:
	case CSPL_MBOX_CMD_UNKNOWN_CMD:
		return true;
	case CSPL_MBOX_CMD_PAUSE:
		return (sts == CSPL_MBOX_STS_PAUSED);
	case CSPL_MBOX_CMD_RESUME:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_REINIT:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_STOP_PRE_REINIT:
		return (sts == CSPL_MBOX_STS_RDY_FOR_REINIT);
	case CSPL_MBOX_CMD_HIBERNATE:
		return (sts == CSPL_MBOX_STS_HIBERNATE);
	case CSPL_MBOX_CMD_OUT_OF_HIBERNATE:
		return (sts == CSPL_MBOX_STS_PAUSED);
	default:
		return false;
	}
}

int cs35l45_set_csplmboxcmd(struct cs35l45_private *cs35l45,
			    enum cspl_mboxcmd cmd)
{
	unsigned int sts, i;
	bool ack = false;

	/* Reset DSP sticky bit */
	regmap_write(cs35l45->regmap, CS35L45_IRQ2_EINT_2,
		     CS35L45_DSP_VIRT1_MBOX_MASK);

	/* Reset AP sticky bit */
	regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_2,
		     CS35L45_DSP_VIRT2_MBOX_MASK);

	/* Unmask DSP INT */
	regmap_update_bits(cs35l45->regmap, CS35L45_IRQ2_MASK_2,
			   CS35L45_DSP_VIRT1_MBOX_MASK, 0);

	regmap_write(cs35l45->regmap, CS35L45_DSP_VIRT1_MBOX_1, cmd);

	/* Poll for DSP ACK */
	for (i = 0; i < 5; i++) {
		usleep_range(1000, 1100);

		regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_2, &sts);
		if (!(sts & CS35L45_DSP_VIRT2_MBOX_MASK))
			continue;

		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_2,
			     CS35L45_DSP_VIRT2_MBOX_MASK);

		ack = true;
		break;
	}

	/* Mask DSP INT */
	regmap_update_bits(cs35l45->regmap, CS35L45_IRQ2_MASK_2,
			   CS35L45_DSP_VIRT1_MBOX_MASK,
			   CS35L45_DSP_VIRT1_MBOX_MASK);

	if (!ack) {
		dev_err(cs35l45->dev, "Timeout waiting for MBOX ACK\n");
		return -ETIMEDOUT;
	}

	regmap_read(cs35l45->regmap, CS35L45_DSP_MBOX_2, &sts);
	if (!cs35l45_is_csplmboxsts_correct(cmd, (enum cspl_mboxstate)sts)) {
		dev_err(cs35l45->dev, "Failed to set MBOX (cmd: %u, sts: %u)\n",
			cmd, sts);
		return -ENOMSG;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_set_csplmboxcmd);

static int cs35l45_dsp_loader_ev(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	enum cspl_mboxcmd mboxcmd = CSPL_MBOX_CMD_NONE;
	unsigned int sts, i;
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!cs35l45->dsp.booted) {
			regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
					   CS35L45_MEM_RDY_MASK, 0);

			wm_adsp_early_event(w, kcontrol, event);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!cs35l45->dsp.booted) {
			regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
					   CS35L45_MEM_RDY_MASK,
					   CS35L45_MEM_RDY_MASK);

			regmap_write(cs35l45->regmap,
				     CS35L45_DSP1_CCM_CORE_CONTROL,
				     CS35L45_CCM_PM_REMAP_MASK |
				     CS35L45_CCM_CORE_RESET_MASK);

			wm_adsp_event(w, kcontrol, event);

			/* Poll for DSP ACK */
			for (i = 0; i < 5; i++) {
				usleep_range(1000, 1100);

				regmap_read(cs35l45->regmap,
					    CS35L45_IRQ1_EINT_2, &sts);
				if (!(sts & CS35L45_DSP_VIRT2_MBOX_MASK))
					continue;

				regmap_write(cs35l45->regmap,
					     CS35L45_IRQ1_EINT_2,
					     CS35L45_DSP_VIRT2_MBOX_MASK);

				break;
			}

			if (i == 5) {
				dev_err(cs35l45->dev,
					"Timeout waiting for MBOX ACK\n");
				return -ETIMEDOUT;
			}

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
			cirrus_cal_apply(cs35l45->pdata.mfd_suffix);
#endif

			mboxcmd = CSPL_MBOX_CMD_PAUSE;
			ret = cs35l45_set_csplmboxcmd(cs35l45, mboxcmd);
			if (ret < 0) {
				dev_err(cs35l45->dev, "MBOX failure (%d)\n",
					ret);
				return ret;
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!cs35l45->dsp.preloaded) {
			regmap_update_bits(cs35l45->regmap,
				CS35L45_DSP1_STREAM_ARB_TX1_CONFIG_0,
				CS35L45_DSP1_STREAM_ARB_TX1_EN_MASK, 0);

			regmap_update_bits(cs35l45->regmap,
				CS35L45_DSP1_STREAM_ARB_MSTR1_CONFIG_0,
				CS35L45_DSP1_STREAM_ARB_MSTR0_EN_MASK, 0);

			wm_adsp_early_event(w, kcontrol, event);
			wm_adsp_event(w, kcontrol, event);
		}
		break;
	default:
		dev_err(cs35l45->dev, "Invalid event = 0x%x\n", event);
		return -EINVAL;
	}

	return 0;
}

static int cs35l45_dsp_power_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	enum cspl_mboxcmd mboxcmd = CSPL_MBOX_CMD_NONE;
	enum cspl_mboxstate fw_status = CSPL_MBOX_STS_RUNNING;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (cs35l45->dsp.running) {
			regmap_read(cs35l45->regmap, CS35L45_DSP_MBOX_2,
				    (unsigned int *)&fw_status);

			switch (fw_status) {
			case CSPL_MBOX_STS_RDY_FOR_REINIT:
				mboxcmd = CSPL_MBOX_CMD_REINIT;
				break;
			case CSPL_MBOX_STS_RUNNING: /* First playback */
			case CSPL_MBOX_STS_PAUSED:
				mboxcmd = CSPL_MBOX_CMD_RESUME;
				break;
			default:
				dev_err(cs35l45->dev,
					"Invalid FW status (%u)\n", fw_status);
				return -EINVAL;
			}

			ret = cs35l45_set_csplmboxcmd(cs35l45, mboxcmd);

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
			cirrus_pwr_start(cs35l45->pdata.mfd_suffix);
#endif
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (cs35l45->dsp.running) {
			mboxcmd = CSPL_MBOX_CMD_PAUSE;
			ret = cs35l45_set_csplmboxcmd(cs35l45, mboxcmd);
			if (ret < 0)
				dev_err(cs35l45->dev, "MBOX failure (%d)\n",
					ret);

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
			cirrus_pwr_stop(cs35l45->pdata.mfd_suffix);
#endif
		}
		break;
	default:
		dev_err(cs35l45->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}

	return ret;
}

static int cs35l45_global_en_power_ev(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	unsigned int val;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (cs35l45->amplifier_mode == AMP_MODE_SPK) {
			regmap_update_bits(cs35l45->regmap,
					   CS35L45_BLOCK_ENABLES,
					   CS35L45_BST_EN_MASK,
					   CS35L45_BST_ENABLE <<
					   CS35L45_BST_EN_SHIFT);

			regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
					   CS35L45_HVLV_MODE_MASK,
					   CS35L45_HVLV_OPERATION <<
					   CS35L45_HVLV_MODE_SHIFT);
		} else  /* AMP_MODE_RCV */ {
			regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
					   CS35L45_HVLV_MODE_MASK,
					   CS35L45_FORCE_LV_OPERATION <<
					   CS35L45_HVLV_MODE_SHIFT);

			regmap_update_bits(cs35l45->regmap,
					   CS35L45_BLOCK_ENABLES2,
					   CS35L45_AMP_DRE_EN_MASK, 0);

			regmap_update_bits(cs35l45->regmap, CS35L45_AMP_GAIN,
					   CS35L45_AMP_GAIN_PCM_MASK,
					   CS35L45_AMP_GAIN_PCM_13DBV <<
					   CS35L45_AMP_GAIN_PCM_SHIFT);
		}

		regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_ENABLES,
				CS35L45_GLOBAL_EN_MASK, CS35L45_GLOBAL_EN_MASK);

		usleep_range(3000, 3100);

		if (cs35l45->amplifier_mode == AMP_MODE_RCV)
			regmap_update_bits(cs35l45->regmap,
				CS35L45_BLOCK_ENABLES, CS35L45_BST_EN_MASK,
				CS35L45_BST_DISABLE_FET_ON <<
				CS35L45_BST_EN_SHIFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_read(cs35l45->regmap, CS35L45_BLOCK_ENABLES, &val);

		val = (val & CS35L45_BST_EN_MASK) >> CS35L45_BST_EN_SHIFT;
		if (val == CS35L45_BST_DISABLE_FET_ON)
			regmap_update_bits(cs35l45->regmap,
				CS35L45_BLOCK_ENABLES, CS35L45_BST_EN_MASK,
				CS35L45_BST_DISABLE_FET_OFF <<
				CS35L45_BST_EN_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_ENABLES,
				CS35L45_GLOBAL_EN_MASK, 0);

		usleep_range(1000, 1100);
		break;
	default:
		dev_err(cs35l45->dev, "Invalid event = 0x%x\n", event);
		return -EINVAL;
	}


	return 0;
}

static const char * const pcm_tx_txt[] = {"Zero", "ASP_RX1", "ASP_RX2", "VMON",
			"IMON", "ERR_VOL", "VDD_BATTMON", "VDD_BSTMON",
			"DSP_TX1", "DSP_TX2"};

static const unsigned int pcm_tx_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2,
			CS35L45_PCM_SRC_VMON, CS35L45_PCM_SRC_IMON,
			CS35L45_PCM_SRC_ERR_VOL, CS35L45_PCM_SRC_VDD_BATTMON,
			CS35L45_PCM_SRC_VDD_BSTMON, CS35L45_PCM_SRC_DSP_TX1,
			CS35L45_PCM_SRC_DSP_TX2};

static const char * const pcm_rx_txt[] = {"Zero", "ASP_RX1", "ASP_RX2"};

static const unsigned int pcm_rx_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2};

static const char * const pcm_dac_txt[] = {"Zero", "ASP_RX1", "ASP_RX2",
			"DSP_TX1", "DSP_TX2"};

static const unsigned int pcm_dac_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2,
			CS35L45_PCM_SRC_DSP_TX1, CS35L45_PCM_SRC_DSP_TX2};

static const struct soc_enum mux_enums[] = {
	SOC_VALUE_ENUM_DOUBLE(CS35L45_ASPTX1_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_ASPTX2_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_ASPTX3_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			 ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_ASPTX4_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_DSP1RX1_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_DSP1RX2_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_DOUBLE(CS35L45_DACPCM1_INPUT, 0, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_dac_txt), pcm_dac_txt, pcm_dac_val),
};

static const struct snd_kcontrol_new muxes[] = {
	SOC_DAPM_ENUM("ASP_TX1 Source", mux_enums[ASP_TX1]),
	SOC_DAPM_ENUM("ASP_TX2 Source", mux_enums[ASP_TX2]),
	SOC_DAPM_ENUM("ASP_TX3 Source", mux_enums[ASP_TX3]),
	SOC_DAPM_ENUM("ASP_TX4 Source", mux_enums[ASP_TX4]),
	SOC_DAPM_ENUM("DSP_RX1 Source", mux_enums[DSP_RX1]),
	SOC_DAPM_ENUM("DSP_RX2 Source", mux_enums[DSP_RX2]),
	SOC_DAPM_ENUM("DACPCM Source", mux_enums[DACPCM]),
};

static const struct snd_soc_dapm_widget cs35l45_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("DSP1 Preload", NULL),

	SND_SOC_DAPM_SUPPLY_S("DSP1 Preloader", 100, SND_SOC_NOPM, 0, 0,
			      cs35l45_dsp_loader_ev, SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUT_DRV_E("DSP", SND_SOC_NOPM, 0, 0, NULL, 0,
			       cs35l45_dsp_power_ev, SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("GLOBAL_EN", SND_SOC_NOPM, 0, 0, NULL, 0,
			   cs35l45_global_en_power_ev, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("VMON", CS35L45_BLOCK_ENABLES, 12, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("IMON", CS35L45_BLOCK_ENABLES, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BATTMON", CS35L45_BLOCK_ENABLES, 8, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BSTMON", CS35L45_BLOCK_ENABLES, 9, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN("ASP", NULL, 0, CS35L45_BLOCK_ENABLES2, 27, 0),
	SND_SOC_DAPM_AIF_IN("ASP_RX1", NULL, 0, CS35L45_ASP_ENABLES1, 16, 0),
	SND_SOC_DAPM_AIF_IN("ASP_RX2", NULL, 0, CS35L45_ASP_ENABLES1, 17, 0),

	SND_SOC_DAPM_AIF_OUT("ASP_TX1", NULL, 0, CS35L45_ASP_ENABLES1, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ASP_TX2", NULL, 0, CS35L45_ASP_ENABLES1, 1, 0),
	SND_SOC_DAPM_AIF_OUT("ASP_TX3", NULL, 0, CS35L45_ASP_ENABLES1, 2, 0),
	SND_SOC_DAPM_AIF_OUT("ASP_TX4", NULL, 0, CS35L45_ASP_ENABLES1, 3, 0),

	SND_SOC_DAPM_MUX("ASP_TX1 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX1]),
	SND_SOC_DAPM_MUX("ASP_TX2 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX2]),
	SND_SOC_DAPM_MUX("ASP_TX3 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX3]),
	SND_SOC_DAPM_MUX("ASP_TX4 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX4]),
	SND_SOC_DAPM_MUX("DSP_RX1 Source", SND_SOC_NOPM, 0, 0, &muxes[DSP_RX1]),
	SND_SOC_DAPM_MUX("DSP_RX2 Source", SND_SOC_NOPM, 0, 0, &muxes[DSP_RX2]),
	SND_SOC_DAPM_MUX("DACPCM Source", SND_SOC_NOPM, 0, 0, &muxes[DACPCM]),

	SND_SOC_DAPM_AIF_OUT("RCV_EN", NULL, 0, CS35L45_BLOCK_ENABLES, 2, 0),

	SND_SOC_DAPM_OUTPUT("SPK"),
	SND_SOC_DAPM_OUTPUT("RCV"),
	SND_SOC_DAPM_INPUT("AP"),
};

static const struct snd_soc_dapm_route cs35l45_dapm_routes[] = {
	/* DSP */
	{"DSP1 Preload", NULL, "DSP1 Preloader"},

	{"DSP", NULL, "DSP1 Preloader"},
	{"DSP", NULL, "VMON"},
	{"DSP", NULL, "IMON"},
	{"DSP", NULL, "BATTMON"},
	{"DSP", NULL, "BSTMON"},

	/* Feedback */
	{"ASP_TX1", NULL, "AP"},
	{"ASP_TX2", NULL, "AP"},
	{"ASP_TX3", NULL, "AP"},
	{"ASP_TX4", NULL, "AP"},

	{"ASP_TX1 Source", "ASP_RX1", "ASP_TX1"},
	{"ASP_TX1 Source", "ASP_RX2", "ASP_TX1"},
	{"ASP_TX1 Source", "VMON", "ASP_TX1"},
	{"ASP_TX1 Source", "IMON", "ASP_TX1"},
	{"ASP_TX1 Source", "VDD_BATTMON", "ASP_TX1"},
	{"ASP_TX1 Source", "VDD_BSTMON", "ASP_TX1"},
	{"ASP_TX1 Source", "DSP_TX1", "ASP_TX1"},
	{"ASP_TX1 Source", "DSP_TX2", "ASP_TX1"},

	{"ASP_TX2 Source", "ASP_RX1", "ASP_TX2"},
	{"ASP_TX2 Source", "ASP_RX2", "ASP_TX2"},
	{"ASP_TX2 Source", "VMON", "ASP_TX2"},
	{"ASP_TX2 Source", "IMON", "ASP_TX2"},
	{"ASP_TX2 Source", "VDD_BATTMON", "ASP_TX2"},
	{"ASP_TX2 Source", "VDD_BSTMON", "ASP_TX2"},
	{"ASP_TX2 Source", "DSP_TX1", "ASP_TX2"},
	{"ASP_TX2 Source", "DSP_TX2", "ASP_TX2"},

	{"ASP_TX3 Source", "ASP_RX1", "ASP_TX3"},
	{"ASP_TX3 Source", "ASP_RX2", "ASP_TX3"},
	{"ASP_TX3 Source", "VMON", "ASP_TX3"},
	{"ASP_TX3 Source", "IMON", "ASP_TX3"},
	{"ASP_TX3 Source", "VDD_BATTMON", "ASP_TX3"},
	{"ASP_TX3 Source", "VDD_BSTMON", "ASP_TX3"},
	{"ASP_TX3 Source", "DSP_TX1", "ASP_TX3"},
	{"ASP_TX3 Source", "DSP_TX2", "ASP_TX3"},

	{"ASP_TX4 Source", "ASP_RX1", "ASP_TX4"},
	{"ASP_TX4 Source", "ASP_RX2", "ASP_TX4"},
	{"ASP_TX4 Source", "VMON", "ASP_TX4"},
	{"ASP_TX4 Source", "IMON", "ASP_TX4"},
	{"ASP_TX4 Source", "VDD_BATTMON", "ASP_TX4"},
	{"ASP_TX4 Source", "VDD_BSTMON", "ASP_TX4"},
	{"ASP_TX4 Source", "DSP_TX1", "ASP_TX4"},
	{"ASP_TX4 Source", "DSP_TX2", "ASP_TX4"},

	{"Capture", NULL, "ASP_TX1 Source"},
	{"Capture", NULL, "ASP_TX2 Source"},
	{"Capture", NULL, "ASP_TX3 Source"},
	{"Capture", NULL, "ASP_TX4 Source"},

	{"Capture", NULL, "VMON"},
	{"Capture", NULL, "IMON"},
	{"Capture", NULL, "BATTMON"},
	{"Capture", NULL, "BSTMON"},

	/* Playback */
	{"GLOBAL_EN", NULL, "Playback"},

	{"ASP", NULL, "GLOBAL_EN"},
	{"DSP", NULL, "GLOBAL_EN"},

	{"ASP_RX1", NULL, "ASP"},
	{"ASP_RX2", NULL, "ASP"},

	{"DSP_RX1 Source", "Zero", "GLOBAL_EN"},
	{"DSP_RX1 Source", "ASP_RX1", "ASP_RX1"},
	{"DSP_RX1 Source", "ASP_RX2", "ASP_RX2"},

	{"DSP_RX2 Source", "Zero", "GLOBAL_EN"},
	{"DSP_RX2 Source", "ASP_RX1", "ASP_RX1"},
	{"DSP_RX2 Source", "ASP_RX2", "ASP_RX2"},

	{"DACPCM Source", "Zero", "GLOBAL_EN"},
	{"DACPCM Source", "ASP_RX1", "ASP_RX1"},
	{"DACPCM Source", "ASP_RX2", "ASP_RX2"},
	{"DACPCM Source", "DSP_TX1", "DSP_RX1 Source"},
	{"DACPCM Source", "DSP_TX1", "DSP_RX2 Source"},
	{"DACPCM Source", "DSP_TX2", "DSP_RX1 Source"},
	{"DACPCM Source", "DSP_TX2", "DSP_RX2 Source"},

	{"SPK", NULL, "DACPCM Source"},

	{"RCV_EN", NULL, "DACPCM Source"},
	{"RCV", NULL, "RCV_EN"},
};

static const struct snd_soc_dapm_route cs35l45_dsp_dapm_routes[] = {
	{"DSP_RX1 Source", NULL, "DSP"},
	{"DSP_RX2 Source", NULL, "DSP"},
};

static int cs35l45_activate_ctl(struct cs35l45_private *cs35l45,
				const char *ctl_name, bool active)
{
	struct snd_soc_component *component =
			snd_soc_lookup_component(cs35l45->dev, NULL);
	struct snd_card *card = component->card->snd_card;
	struct snd_kcontrol *kcontrol;
	struct snd_kcontrol_volatile *vd;
	unsigned int index_offset;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];

	if (component->name_prefix)
		snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN, "%s %s",
			 component->name_prefix, ctl_name);
	else
		snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN, "%s", ctl_name);

	kcontrol = snd_soc_card_get_kcontrol(component->card, name);
	if (!kcontrol) {
		dev_err(cs35l45->dev, "Can't find kcontrol %s\n", name);
		return -EINVAL;
	}

	index_offset = snd_ctl_get_ioff(kcontrol, &kcontrol->id);
	vd = &kcontrol->vd[index_offset];
	if (active)
		vd->access |= SNDRV_CTL_ELEM_ACCESS_WRITE;
	else
		vd->access &= ~SNDRV_CTL_ELEM_ACCESS_WRITE;

	snd_ctl_notify(card, SNDRV_CTL_EVENT_MASK_INFO, &kcontrol->id);

	return 0;
}

static const char * const gain_texts[] = {"10dB", "13dB", "16dB", "19dB"};
static const unsigned int gain_values[] = {0x00, 0x01, 0x02, 0x03};
static SOC_VALUE_ENUM_SINGLE_DECL(gain_enum, CS35L45_AMP_GAIN,
			CS35L45_AMP_GAIN_PCM_SHIFT,
			CS35L45_AMP_GAIN_PCM_MASK >> CS35L45_AMP_GAIN_PCM_SHIFT,
			gain_texts, gain_values);

static const char * const amplifier_mode_texts[] = {"SPK", "RCV"};
static SOC_ENUM_SINGLE_DECL(amplifier_mode_enum, SND_SOC_NOPM, 0,
			    amplifier_mode_texts);

static const char * const hibernate_mode_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(hibernate_mode_enum, SND_SOC_NOPM, 0,
			    hibernate_mode_texts);

static int cs35l45_amplifier_mode_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l45->amplifier_mode;

	return 0;
}

static int cs35l45_amplifier_mode_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int status;

	if (ucontrol->value.integer.value[0] == cs35l45->amplifier_mode)
		return 0;

	cs35l45->amplifier_mode = ucontrol->value.integer.value[0];

	status = snd_soc_component_get_pin_status(component, "SPK") |
		 snd_soc_component_get_pin_status(component, "RCV");

	/* Ensure device is disabled before switching amplifier mode */
	snd_soc_component_disable_pin(component, "SPK");
	snd_soc_component_disable_pin(component, "RCV");

	snd_soc_dapm_sync(dapm);

	if (cs35l45->amplifier_mode == AMP_MODE_SPK)
		snd_soc_dapm_add_routes(dapm, cs35l45_dsp_dapm_routes,
					ARRAY_SIZE(cs35l45_dsp_dapm_routes));
	else /* AMP_MODE_RCV */
		snd_soc_dapm_del_routes(dapm, cs35l45_dsp_dapm_routes,
					ARRAY_SIZE(cs35l45_dsp_dapm_routes));

	/* If playback is not in progress, exit */
	if (!status)
		goto skip_pin_enable;

	if (cs35l45->amplifier_mode == AMP_MODE_SPK)
		snd_soc_component_enable_pin(component, "SPK");
	else /* AMP_MODE_RCV */
		snd_soc_component_enable_pin(component, "RCV");

skip_pin_enable:
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int cs35l45_hibernate_mode_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l45->hibernate_mode;

	return 0;
}

static int cs35l45_hibernate_mode_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	return cs35l45_hibernate(cs35l45, ucontrol->value.integer.value[0]);
}

static const struct snd_kcontrol_new cs35l45_aud_controls[] = {
	WM_ADSP2_PRELOAD_SWITCH("DSP1", 1),
	WM_ADSP_FW_CONTROL("DSP1", 0),
	SOC_ENUM_EXT("Amplifier Mode", amplifier_mode_enum,
		     cs35l45_amplifier_mode_get, cs35l45_amplifier_mode_put),
	SOC_ENUM("AMP PCM Gain", gain_enum),
	SOC_SINGLE("AMP Mute", CS35L45_AMP_OUTPUT_MUTE, 0, 1, 0),
	SOC_ENUM_EXT("Hibernate Mode", hibernate_mode_enum,
		     cs35l45_hibernate_mode_get, cs35l45_hibernate_mode_put),
};

static int cs35l45_dai_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(codec_dai->component);
	unsigned int asp_fmt, fsync_inv, bclk_inv, master_mode;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		master_mode = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		master_mode = 0;
		break;
	default:
		dev_warn(cs35l45->dev, "Mixed master mode unsupported (%d)\n",
			 fmt & SND_SOC_DAIFMT_MASTER_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_BCLK_MSTR_MASK,
			   master_mode << CS35L45_ASP_BCLK_MSTR_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FSYNC_MSTR_MASK,
			   master_mode << CS35L45_ASP_FSYNC_MSTR_SHIFT);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		asp_fmt = 0;
		break;
	case SND_SOC_DAIFMT_I2S:
		asp_fmt = 2;
		break;
	default:
		dev_warn(cs35l45->dev, "Unsupported DAI format (%d)\n",
			 fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FMT_MASK,
			   asp_fmt << CS35L45_ASP_FMT_SHIFT);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_IF:
		fsync_inv = 1;
		bclk_inv = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		fsync_inv = 0;
		bclk_inv = 1;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		fsync_inv = 1;
		bclk_inv = 1;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		fsync_inv = 0;
		bclk_inv = 0;
		break;
	default:
		dev_warn(cs35l45->dev, "Invalid clock polarity (%d)\n",
			 fmt & SND_SOC_DAIFMT_INV_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FSYNC_INV_MASK,
			   fsync_inv << CS35L45_ASP_FSYNC_INV_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_BCLK_INV_MASK,
			   bclk_inv << CS35L45_ASP_BCLK_INV_SHIFT);

	return 0;
}

static int cs35l45_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);
	unsigned int asp_width, asp_wl, global_fs;

	switch (params_rate(params)) {
	case 8000:
		global_fs = CS35L45_8_KHZ;
		break;
	case 16000:
		global_fs = CS35L45_16_KHZ;
		break;
	case 44100:
		global_fs = CS35L45_44P100_KHZ;
		break;
	case 48000:
		global_fs = CS35L45_48P0_KHZ;
		break;
	case 88200:
		global_fs = CS35L45_88P200_KHZ;
		break;
	case 96000:
		global_fs = CS35L45_96P0_KHZ;
		break;
	default:
		dev_warn(cs35l45->dev, "Unsupported params rate (%d)\n",
			 params_rate(params));
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_SAMPLE_RATE,
			   CS35L45_GLOBAL_FS_MASK,
			   global_fs << CS35L45_GLOBAL_FS_SHIFT);

	asp_wl = params_width(params);
	asp_width = cs35l45->pdata.use_tdm_slots ?
			    cs35l45->slot_width : params_physical_width(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
				   CS35L45_ASP_WIDTH_RX_MASK,
				   asp_width << CS35L45_ASP_WIDTH_RX_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_DATA_CONTROL5,
				   CS35L45_ASP_WL_MASK,
				   asp_wl << CS35L45_ASP_WL_SHIFT);
	} else {
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
				   CS35L45_ASP_WIDTH_TX_MASK,
				   asp_width << CS35L45_ASP_WIDTH_TX_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_DATA_CONTROL1,
				   CS35L45_ASP_WL_MASK,
				   asp_wl << CS35L45_ASP_WL_SHIFT);
	}

	return 0;
}

static int cs35l45_dai_set_tdm_slot(struct snd_soc_dai *dai,
				    unsigned int tx_mask, unsigned int rx_mask,
				    int slots, int slot_width)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);

	cs35l45->slot_width = slot_width;

	return 0;
}

static int cs35l45_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);

	return cs35l45_set_sysclk(cs35l45, clk_id, freq);
}

static int cs35l45_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);

	if (cs35l45->hibernate_mode == HIBER_MODE_EN) {
		dev_err(cs35l45->dev,
			"Amp is hibernating; please wake up first\n");
		return -EPERM;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!cs35l45->amplifier_mode)
			snd_soc_component_enable_pin(component, "SPK");
		else
			snd_soc_component_enable_pin(component, "RCV");
	} else {
		snd_soc_component_enable_pin(component, "AP");
	}

	snd_soc_dapm_sync(dapm);

	return 0;
}

static void cs35l45_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_component_disable_pin(component, "SPK");
		snd_soc_component_disable_pin(component, "RCV");
	} else {
		snd_soc_component_disable_pin(component, "AP");
	}

	snd_soc_dapm_sync(dapm);
}

static const struct snd_soc_dai_ops cs35l45_dai_ops = {
	.startup = cs35l45_dai_startup,
	.shutdown = cs35l45_dai_shutdown,
	.set_fmt = cs35l45_dai_set_fmt,
	.hw_params = cs35l45_dai_hw_params,
	.set_tdm_slot = cs35l45_dai_set_tdm_slot,
	.set_sysclk = cs35l45_dai_set_sysclk,
};

#define CS35L45_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE| \
			 SNDRV_PCM_FMTBIT_S24_LE)

#define CS35L45_RATES (SNDRV_PCM_RATE_8000  | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000 | \
		       SNDRV_PCM_RATE_88200 | \
		       SNDRV_PCM_RATE_96000)

static struct snd_soc_dai_driver cs35l45_dai = {
	.name = "cs35l45",
	.playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = CS35L45_RATES,
		      .formats = CS35L45_FORMATS,
	},
	.capture = {
		      .stream_name = "Capture",
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = CS35L45_RATES,
		      .formats = CS35L45_FORMATS,
	},
	.ops = &cs35l45_dai_ops,
};

static int cs35l45_component_set_sysclk(struct snd_soc_component *component,
					int clk_id, int source,
					unsigned int freq, int dir)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	return cs35l45_set_sysclk(cs35l45, clk_id, freq);
}

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
#define CS35L45_ALG_ID_VIMON	0xf205
#endif

static int cs35l45_component_probe(struct snd_soc_component *component)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
#ifdef CONFIG_SND_SOC_CIRRUS_AMP
	static struct reg_sequence cs35l45_cal_pre_config[] = {
		{CS35L45_DSP1RX6_INPUT,	CS35L45_PCM_SRC_VDD_BSTMON},
	};

	static struct reg_sequence cs35l45_cal_post_config[] = {
		{CS35L45_DSP1RX6_INPUT,	CS35L45_PCM_SRC_VDD_BATTMON},
	};

	struct cirrus_amp_config amp_cfg;
	int ret;

	amp_cfg.component = component;
	amp_cfg.regmap = cs35l45->regmap;
	amp_cfg.pre_config = cs35l45_cal_pre_config;
	amp_cfg.post_config = cs35l45_cal_post_config;
	amp_cfg.dsp_part_name = cs35l45->pdata.dsp_part_name;
	amp_cfg.num_pre_configs = ARRAY_SIZE(cs35l45_cal_pre_config);
	amp_cfg.num_post_configs = ARRAY_SIZE(cs35l45_cal_post_config);
	amp_cfg.mbox_cmd = CS35L45_DSP_VIRT1_MBOX_1;
	amp_cfg.mbox_sts = CS35L45_DSP_MBOX_2;
	amp_cfg.global_en = CS35L45_GLOBAL_ENABLES;
	amp_cfg.vimon_alg_id = CS35L45_ALG_ID_VIMON;
	amp_cfg.bd_max_temp = cs35l45->pdata.bd_max_temp &
			      (~CS35L45_VALID_PDATA);
	amp_cfg.target_temp = cs35l45->pdata.pwr_params_cfg.target_temp &
			      (~CS35L45_VALID_PDATA);
	amp_cfg.exit_temp = cs35l45->pdata.pwr_params_cfg.exit_temp &
			    (~CS35L45_VALID_PDATA);
	amp_cfg.perform_vimon_cal = false;

	ret = cirrus_amp_add(cs35l45->pdata.mfd_suffix, amp_cfg);
	if (ret < 0) {
		dev_err(cs35l45->dev, "Failed to register cirrus amp (%d)\n",
			ret);
		return -EPROBE_DEFER;
	}
#endif

	snd_soc_component_disable_pin(component, "SPK");
	snd_soc_component_disable_pin(component, "RCV");
	snd_soc_component_disable_pin(component, "AP");

	snd_soc_dapm_add_routes(dapm, cs35l45_dsp_dapm_routes,
				ARRAY_SIZE(cs35l45_dsp_dapm_routes));

	snd_soc_dapm_sync(dapm);

	component->regmap = cs35l45->regmap;

	return wm_adsp2_component_probe(&cs35l45->dsp, component);
}

static void cs35l45_component_remove(struct snd_soc_component *component)
{
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	wm_adsp2_component_remove(&cs35l45->dsp, component);
}

static const struct snd_soc_component_driver cs35l45_component = {
	.probe = cs35l45_component_probe,
	.remove = cs35l45_component_remove,
	.set_sysclk = cs35l45_component_set_sysclk,

	.dapm_widgets = cs35l45_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l45_dapm_widgets),

	.dapm_routes = cs35l45_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(cs35l45_dapm_routes),

	.controls = cs35l45_aud_controls,
	.num_controls = ARRAY_SIZE(cs35l45_aud_controls),
};

static int cs35l45_get_clk_config(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l45_pll_sysclk); i++) {
		if (cs35l45_pll_sysclk[i].freq == freq)
			return cs35l45_pll_sysclk[i].clk_cfg;
	}

	return -EINVAL;
}

static int cs35l45_set_sysclk(struct cs35l45_private *cs35l45, int clk_id,
			      unsigned int freq)
{
	int extclk_cfg, clksrc;


	switch (clk_id) {
	case 0:
		clksrc = CS35L45_PLL_REFCLK_SEL_BCLK;
		break;
	default:
		dev_err(cs35l45->dev, "Invalid CLK Config\n");
		return -EINVAL;
	}

	extclk_cfg = cs35l45_get_clk_config(freq);
	if (extclk_cfg < 0) {
		dev_err(cs35l45->dev, "Invalid CLK Config: %d, freq: %u\n",
			extclk_cfg, freq);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_OPEN_LOOP_MASK,
			   CS35L45_PLL_OPEN_LOOP_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_FREQ_MASK,
			   extclk_cfg << CS35L45_PLL_REFCLK_FREQ_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_SEL_MASK,
			   clksrc << CS35L45_PLL_REFCLK_SEL_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_OPEN_LOOP_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_EN_MASK,
			   CS35L45_PLL_REFCLK_EN_MASK);

	return 0;
}

static const struct cs35l45_irq_monitor cs35l45_irq_mons[] = {
	{
		.reg = CS35L45_IRQ1_EINT_3,
		.mask = CS35L45_IRQ1_MASK_3,
		.bitmask = CS35L45_PLL_UNLOCK_FLAG_RISE_MASK,
		.description = "PLL unlock (rising edge)",
		.callback = NULL,
	},
	{
		.reg = CS35L45_IRQ1_EINT_3,
		.mask = CS35L45_IRQ1_MASK_3,
		.bitmask = CS35L45_PLL_LOCK_FLAG_MASK,
		.description = "PLL lock",
		.callback = NULL,
	},
};

static irqreturn_t cs35l45_irq(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;
	unsigned int irq_regs[] = {CS35L45_IRQ1_EINT_1, CS35L45_IRQ1_EINT_2,
				   CS35L45_IRQ1_EINT_3, CS35L45_IRQ1_EINT_4,
				   CS35L45_IRQ1_EINT_5, CS35L45_IRQ1_EINT_7,
				   CS35L45_IRQ1_EINT_8, CS35L45_IRQ1_EINT_18};
	unsigned int irq_masks[] = {CS35L45_IRQ1_MASK_1, CS35L45_IRQ1_MASK_2,
				    CS35L45_IRQ1_MASK_3, CS35L45_IRQ1_MASK_4,
				    CS35L45_IRQ1_MASK_5, CS35L45_IRQ1_MASK_7,
				    CS35L45_IRQ1_MASK_8, CS35L45_IRQ1_MASK_18};
	unsigned int status[ARRAY_SIZE(irq_regs)];
	unsigned int masks[ARRAY_SIZE(irq_masks)];
	unsigned int val;
	unsigned int i;
	int ret;
	bool irq_detect = false;

	if (!cs35l45->initialized)
		return IRQ_NONE;

	for (i = 0; i < ARRAY_SIZE(irq_regs); i++) {
		regmap_read(cs35l45->regmap, irq_regs[i], &status[i]);
		regmap_read(cs35l45->regmap, irq_masks[i], &masks[i]);
		irq_detect |= (status[i] & (~masks[i]));
	}

	if (!irq_detect)
		return IRQ_NONE;

	for (i = 0; i < ARRAY_SIZE(cs35l45_irq_mons); i++) {
		regmap_read(cs35l45->regmap, cs35l45_irq_mons[i].reg, &val);
		if (!(val & cs35l45_irq_mons[i].bitmask))
			continue;

		regmap_write(cs35l45->regmap, cs35l45_irq_mons[i].reg,
			     cs35l45_irq_mons[i].bitmask);

		if (cs35l45_irq_mons[i].callback) {
			ret = cs35l45_irq_mons[i].callback(cs35l45);
			if (ret < 0)
				dev_err(cs35l45->dev,
					"IRQ (%s) callback failure (%d)\n",
					cs35l45_irq_mons[i].description, ret);
		}
	}

	return IRQ_HANDLED;
}

static int cs35l45_register_irq_monitors(struct cs35l45_private *cs35l45)
{
	unsigned int val;
	int i;

	if (!cs35l45->irq)
		return 0;

	for (i = 0; i < ARRAY_SIZE(cs35l45_irq_mons); i++) {
		regmap_read(cs35l45->regmap, cs35l45_irq_mons[i].mask, &val);
		if (!(val & cs35l45_irq_mons[i].bitmask)) {
			dev_err(cs35l45->dev, "IRQ (%s) is already unmasked\n",
				cs35l45_irq_mons[i].description);
			continue;
		}

		regmap_write(cs35l45->regmap, cs35l45_irq_mons[i].reg,
			     cs35l45_irq_mons[i].bitmask);

		regmap_update_bits(cs35l45->regmap, cs35l45_irq_mons[i].mask,
				   cs35l45_irq_mons[i].bitmask, 0);
	}

	return 0;
}

static const struct of_entry bst_bpe_v_map[BST_BPE_VOLTAGE_PARAMS] = {
	[L1] = {"bst-bpe-response-l1", CS35L45_BST_BPE_INST_THLD,
		CS35L45_BST_BPE_INST_L0_THLD_MASK,
		CS35L45_BST_BPE_INST_L0_THLD_SHIFT},
	[L2] = {"bst-bpe-response-l2", CS35L45_BST_BPE_INST_THLD,
		CS35L45_BST_BPE_INST_L1_THLD_MASK,
		CS35L45_BST_BPE_INST_L1_THLD_SHIFT},
	[L3] = {"bst-bpe-response-l3", CS35L45_BST_BPE_INST_THLD,
		CS35L45_BST_BPE_INST_L2_THLD_MASK,
		CS35L45_BST_BPE_INST_L2_THLD_SHIFT},
	[L4] = {"bst-bpe-response-l4", CS35L45_BST_BPE_INST_THLD,
		CS35L45_BST_BPE_INST_L3_THLD_MASK,
		CS35L45_BST_BPE_INST_L3_THLD_SHIFT},
};

static const struct of_entry bst_bpe_i_map[BST_BPE_VOLTAGE_PARAMS] = {
	[L1] = {"bst-bpe-response-l1", CS35L45_BST_BPE_INST_ILIM,
		CS35L45_BST_BPE_INST_L1_ILIM_MASK,
		CS35L45_BST_BPE_INST_L1_ILIM_SHIFT},
	[L2] = {"bst-bpe-response-l2", CS35L45_BST_BPE_INST_ILIM,
		CS35L45_BST_BPE_INST_L2_ILIM_MASK,
		CS35L45_BST_BPE_INST_L2_ILIM_SHIFT},
	[L3] = {"bst-bpe-response-l3", CS35L45_BST_BPE_INST_ILIM,
		CS35L45_BST_BPE_INST_L3_ILIM_MASK,
		CS35L45_BST_BPE_INST_L3_ILIM_SHIFT},
	[L4] = {"bst-bpe-response-l4", CS35L45_BST_BPE_INST_ILIM,
		CS35L45_BST_BPE_INST_L4_ILIM_MASK,
		CS35L45_BST_BPE_INST_L4_ILIM_SHIFT},
};

static const struct of_entry bst_bpe_ind_curr_map[BST_BPE_IND_CURR_PARAMS] = {
	[BST_BPE_IL_LIM_THLD_DEL1] = {"bst-bpe-il-lim-thld-del1",
				      CS35L45_BST_BPE_IL_LIM_THLD,
				      CS35L45_BST_BPE_IL_LIM_THLD_DEL1_MASK,
				      CS35L45_BST_BPE_IL_LIM_THLD_DEL1_SHIFT},
	[BST_BPE_IL_LIM_THLD_DEL2] = {"bst-bpe-il-lim-thld-del2",
				      CS35L45_BST_BPE_IL_LIM_THLD,
				      CS35L45_BST_BPE_IL_LIM_THLD_DEL2_MASK,
				      CS35L45_BST_BPE_IL_LIM_THLD_DEL2_SHIFT},
	[BST_BPE_IL_LIM1_THLD] = {"bst-bpe-il-lim1-thld",
				  CS35L45_BST_BPE_IL_LIM_THLD,
				  CS35L45_BST_BPE_IL_LIM1_THLD_MASK,
				  CS35L45_BST_BPE_IL_LIM1_THLD_SHIFT},
	[BST_BPE_IL_LIM1_DLY] = {"bst-bpe-il-lim1-dly",
				 CS35L45_BST_BPE_IL_LIM_DLY,
				 CS35L45_BST_BPE_IL_LIM1_DLY_MASK,
				 CS35L45_BST_BPE_IL_LIM1_DLY_SHIFT},
	[BST_BPE_IL_LIM2_DLY] = {"bst-bpe-il-lim2-dly",
				 CS35L45_BST_BPE_IL_LIM_DLY,
				 CS35L45_BST_BPE_IL_LIM2_DLY_MASK,
				 CS35L45_BST_BPE_IL_LIM2_DLY_SHIFT},
	[BST_BPE_IL_LIM_DLY_HYST] = {"bst-bpe-il-lim-dly-hyst",
				     CS35L45_BST_BPE_IL_LIM_DLY,
				     CS35L45_BST_BPE_IL_LIM_DLY_HYST_MASK,
				     CS35L45_BST_BPE_IL_LIM_DLY_HYST_SHIFT},
	[BST_BPE_IL_LIM_THLD_HYST] = {"bst-bpe-il-lim-thld-hyst",
				      CS35L45_BST_BPE_IL_LIM_THLD,
				      CS35L45_BST_BPE_IL_LIM_THLD_HYST_MASK,
				      CS35L45_BST_BPE_IL_LIM_THLD_HYST_SHIFT},
	[BST_BPE_IL_LIM1_ATK_RATE] = {"bst-bpe-il-lim1-atk-rate",
				      CS35L45_BST_BPE_IL_LIM_ATK_RATE,
				      CS35L45_BST_BPE_IL_LIM1_ATK_RATE_MASK,
				      CS35L45_BST_BPE_IL_LIM1_ATK_RATE_SHIFT},
	[BST_BPE_IL_LIM2_ATK_RATE] = {"bst-bpe-il-lim2-atk-rate",
				      CS35L45_BST_BPE_IL_LIM_ATK_RATE,
				      CS35L45_BST_BPE_IL_LIM2_ATK_RATE_MASK,
				      CS35L45_BST_BPE_IL_LIM2_ATK_RATE_SHIFT},
	[BST_BPE_IL_LIM1_RLS_RATE] = {"bst-bpe-il-lim1-rls-rate",
				      CS35L45_BST_BPE_IL_LIM_RLS_RATE,
				      CS35L45_BST_BPE_IL_LIM1_RLS_RATE_MASK,
				      CS35L45_BST_BPE_IL_LIM1_RLS_RATE_SHIFT},
	[BST_BPE_IL_LIM2_RLS_RATE] = {"bst-bpe-il-lim2-rls-rate",
				      CS35L45_BST_BPE_IL_LIM_RLS_RATE,
				      CS35L45_BST_BPE_IL_LIM2_RLS_RATE_MASK,
				      CS35L45_BST_BPE_IL_LIM2_RLS_RATE_SHIFT},
};

static const struct of_entry ldpm_map[LDPM_PARAMS] = {
	[LDPM_GP1_BOOST_SEL] = {"ldpm-gp1-boost-sel", CS35L45_LDPM_CONFIG,
				CS35L45_LDPM_GP1_BOOST_SEL_MASK,
				CS35L45_LDPM_GP1_BOOST_SEL_SHIFT},
	[LDPM_GP1_AMP_SEL] = {"ldpm-gp1-amp-sel", CS35L45_LDPM_CONFIG,
			      CS35L45_LDPM_GP1_AMP_SEL_MASK,
			      CS35L45_LDPM_GP1_AMP_SEL_SHIFT},
	[LDPM_GP1_DELAY] = {"ldpm-gp1-delay", CS35L45_LDPM_CONFIG,
			    CS35L45_LDPM_GP1_DELAY_MASK,
			    CS35L45_LDPM_GP1_DELAY_SHIFT},
	[LDPM_GP1_PCM_THLD] = {"ldpm-gp1-pcm-thld", CS35L45_LDPM_CONFIG,
			       CS35L45_LDPM_GP1_PCM_THLD_MASK,
			       CS35L45_LDPM_GP1_PCM_THLD_SHIFT},
	[LDPM_GP2_IMON_SEL] = {"ldpm-gp2-imon-sel", CS35L45_LDPM_CONFIG,
			       CS35L45_LDPM_GP2_IMON_SEL_MASK,
			       CS35L45_LDPM_GP2_IMON_SEL_SHIFT},
	[LDPM_GP2_VMON_SEL] = {"ldpm-gp2-vmon-sel", CS35L45_LDPM_CONFIG,
			       CS35L45_LDPM_GP2_VMON_SEL_MASK,
			       CS35L45_LDPM_GP2_VMON_SEL_SHIFT},
	[LDPM_GP2_DELAY] = {"ldpm-gp2-delay", CS35L45_LDPM_CONFIG,
			    CS35L45_LDPM_GP2_DELAY_MASK,
			    CS35L45_LDPM_GP2_DELAY_SHIFT},
	[LDPM_GP2_PCM_THLD] = {"ldpm-gp2-pcm-thld", CS35L45_LDPM_CONFIG,
			       CS35L45_LDPM_GP2_PCM_THLD_MASK,
			       CS35L45_LDPM_GP2_PCM_THLD_SHIFT},
};

static const struct of_entry classh_map[CLASSH_PARAMS] = {
	[CH_HDRM] = {"ch-hdrm", CS35L45_CLASSH_CONFIG1,
		CS35L45_CH_HDRM_MASK, CS35L45_CH_HDRM_SHIFT},
	[CH_RATIO] = {"ch-ratio", CS35L45_CLASSH_CONFIG1,
		CS35L45_CH_RATIO_MASK, CS35L45_CH_RATIO_SHIFT},
	[CH_REL_RATE] = {"ch-rel-rate", CS35L45_CLASSH_CONFIG1,
		CS35L45_CH_REL_RATE_MASK, CS35L45_CH_REL_RATE_SHIFT},
	[CH_OVB_THLD1] = {"ch-ovb-thld1", CS35L45_CLASSH_CONFIG2,
		CS35L45_CH_OVB_THLD1_MASK, CS35L45_CH_OVB_THLD1_SHIFT},
	[CH_OVB_THLDDELTA] = {"ch-ovb-thlddelta", CS35L45_CLASSH_CONFIG2,
		CS35L45_CH_OVB_THLDDELTA_MASK, CS35L45_CH_OVB_THLDDELTA_SHIFT},
	[CH_VDD_BST_MAX] = {"ch-vdd-bst-max", CS35L45_CLASSH_CONFIG2,
		CS35L45_CH_VDD_BST_MAX_MASK, CS35L45_CH_VDD_BST_MAX_SHIFT},
	[CH_OVB_LATCH] = {"ch-ovb-latch", CS35L45_CLASSH_CONFIG3,
		CS35L45_CH_OVB_LATCH_MASK, CS35L45_CH_OVB_LATCH_SHIFT},
	[CH_OVB_RATIO] = {"ch-ovb-ratio", CS35L45_CLASSH_CONFIG3,
		CS35L45_CH_OVB_RATIO_MASK, CS35L45_CH_OVB_RATIO_SHIFT},
	[CH_THLD1_OFFSET] = {"ch-thld1-offset", CS35L45_CLASSH_CONFIG3,
		CS35L45_CH_THLD1_OFFSET_MASK, CS35L45_CH_THLD1_OFFSET_SHIFT},
	[AUD_MEM_DEPTH] = {"aud-mem-depth", CS35L45_AUD_MEM,
		CS35L45_AUD_MEM_DEPTH_MASK, CS35L45_AUD_MEM_DEPTH_SHIFT},
};

static int cs35l45_apply_of_data(struct cs35l45_private *cs35l45)
{
	struct cs35l45_platform_data *pdata = &cs35l45->pdata;
	struct bst_bpe_iv_pair *iv_pairs[] = {&pdata->bst_bpe_voltage_cfg.l1,
					      &pdata->bst_bpe_voltage_cfg.l2,
					      &pdata->bst_bpe_voltage_cfg.l3,
					      &pdata->bst_bpe_voltage_cfg.l4};
	struct gpio_ctrl *gpios[] = {&pdata->gpio_ctrl1, &pdata->gpio_ctrl2,
				     &pdata->gpio_ctrl3};
	unsigned int gpio_regs[] = {CS35L45_GPIO1_CTRL1, CS35L45_GPIO2_CTRL1,
				    CS35L45_GPIO3_CTRL1};
	unsigned int pad_regs[] = {CS35L45_SYNC_GPIO1,
				   CS35L45_INTB_GPIO2_MCLK_REF, CS35L45_GPIO3};
	unsigned int val;
	u32 *ptr;
	int i;

	if (!pdata)
		return 0;

	if (pdata->asp_sdout_hiz_ctrl & CS35L45_VALID_PDATA) {
		val = pdata->asp_sdout_hiz_ctrl & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL3,
				   CS35L45_ASP_DOUT_HIZ_CTRL_MASK,
				   val << CS35L45_ASP_DOUT_HIZ_CTRL_SHIFT);
	}

	if (!pdata->bst_bpe_voltage_cfg.is_present)
		goto bst_bpe_ind_curr_cfg;

	for (i = L1; i < BST_BPE_VOLTAGE_PARAMS; i++) {
		ptr = &iv_pairs[i]->volt;
		val = ((*ptr) & (~CS35L45_VALID_PDATA))
			<< bst_bpe_v_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap,
					   bst_bpe_v_map[i].reg,
					   bst_bpe_v_map[i].mask, val);

		ptr = &iv_pairs[i]->amp;
		val = ((*ptr) & (~CS35L45_VALID_PDATA))
			<< bst_bpe_i_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap,
					   bst_bpe_i_map[i].reg,
					   bst_bpe_i_map[i].mask, val);
	}

bst_bpe_ind_curr_cfg:
	if (!pdata->bst_bpe_ind_curr_cfg.is_present)
		goto hvlv_cfg;

	for (i = BST_BPE_IL_LIM_THLD_DEL1; i < BST_BPE_IND_CURR_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_ind_curr_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA))
			<< bst_bpe_ind_curr_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap,
					   bst_bpe_ind_curr_map[i].reg,
					   bst_bpe_ind_curr_map[i].mask, val);
	}

hvlv_cfg:
	if (!pdata->hvlv_cfg.is_present)
		goto ldpm_cfg;

	if (pdata->hvlv_cfg.hvlv_thld_hys & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_thld_hys & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_THLD_HYS_MASK,
				   val << CS35L45_HVLV_THLD_HYS_SHIFT);
	}

	if (pdata->hvlv_cfg.hvlv_thld & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_thld & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_THLD_MASK,
				   val << CS35L45_HVLV_THLD_SHIFT);
	}

	if (pdata->hvlv_cfg.hvlv_dly & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_dly & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_DLY_MASK,
				   val << CS35L45_HVLV_DLY_SHIFT);
	}

ldpm_cfg:
	if (!pdata->ldpm_cfg.is_present)
		goto classh_cfg;

	for (i = LDPM_GP1_BOOST_SEL; i < LDPM_PARAMS; i++) {
		ptr = cs35l45_get_ldpm_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA)) << classh_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap, classh_map[i].reg,
					   classh_map[i].mask, val);
	}

classh_cfg:
	if (!pdata->classh_cfg.is_present)
		goto gpio_cfg;

	for (i = CH_HDRM; i < CLASSH_PARAMS; i++) {
		ptr = cs35l45_get_classh_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA)) << classh_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap, classh_map[i].reg,
					   classh_map[i].mask, val);
	}

gpio_cfg:
	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (!gpios[i]->is_present)
			continue;

		if (gpios[i]->dir & CS35L45_VALID_PDATA) {
			val = gpios[i]->dir & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_DIR_MASK,
					   val << CS35L45_GPIO_DIR_SHIFT);
		}

		if (gpios[i]->lvl & CS35L45_VALID_PDATA) {
			val = gpios[i]->lvl & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_LVL_MASK,
					   val << CS35L45_GPIO_LVL_SHIFT);
		}

		if (gpios[i]->op_cfg & CS35L45_VALID_PDATA) {
			val = gpios[i]->op_cfg & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_OP_CFG_MASK,
					   val << CS35L45_GPIO_OP_CFG_SHIFT);
		}

		if (gpios[i]->pol & CS35L45_VALID_PDATA) {
			val = gpios[i]->pol & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_POL_MASK,
					   val << CS35L45_GPIO_POL_SHIFT);
		}

		if (gpios[i]->ctrl & CS35L45_VALID_PDATA) {
			val = gpios[i]->ctrl & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, pad_regs[i],
					   CS35L45_GPIO_CTRL_MASK,
					   val << CS35L45_GPIO_CTRL_SHIFT);
		}

		if (gpios[i]->invert & CS35L45_VALID_PDATA) {
			val = gpios[i]->invert & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, pad_regs[i],
					   CS35L45_GPIO_INVERT_MASK,
					   val << CS35L45_GPIO_INVERT_SHIFT);
		}
	}

	return 0;
}

static int cs35l45_parse_of_data(struct cs35l45_private *cs35l45)
{
	struct cs35l45_platform_data *pdata = &cs35l45->pdata;
	struct device_node *node = cs35l45->dev->of_node;
	struct device_node *child;
	struct bst_bpe_iv_pair *iv_pairs[] = {&pdata->bst_bpe_voltage_cfg.l1,
					      &pdata->bst_bpe_voltage_cfg.l2,
					      &pdata->bst_bpe_voltage_cfg.l3,
					      &pdata->bst_bpe_voltage_cfg.l4};
	struct gpio_ctrl *gpios[] = {&pdata->gpio_ctrl1, &pdata->gpio_ctrl2,
				     &pdata->gpio_ctrl3};
	unsigned int val, pair[2];
	char of_name[32];
	u32 *ptr;
	int ret, i;

	if ((!node) || (!pdata))
		return 0;

	ret = of_property_read_u32(node, "cirrus,asp-sdout-hiz-ctrl", &val);
	if (!ret)
		pdata->asp_sdout_hiz_ctrl = val | CS35L45_VALID_PDATA;

	pdata->use_tdm_slots = of_property_read_bool(node,
						     "cirrus,use-tdm-slots");

	ret = of_property_read_string(node, "cirrus,dsp-part-name",
				      &pdata->dsp_part_name);
	if (ret < 0)
		pdata->dsp_part_name = "cs35l45";

#ifdef CONFIG_SND_SOC_CIRRUS_AMP
	ret = of_property_read_string(node, "cirrus,mfd-suffix",
				      &pdata->mfd_suffix);
	if (ret < 0)
		pdata->mfd_suffix = "";

	ret = of_property_read_u32(node, "cirrus,bd-max-temp", &val);
	if (!ret)
		pdata->bd_max_temp = val | CS35L45_VALID_PDATA;

	child = of_get_child_by_name(node, "cirrus,pwr-params-config");
	pdata->pwr_params_cfg.is_present = child ? true : false;
	if (!pdata->pwr_params_cfg.is_present)
		goto bst_bpe_voltage_cfg;

	pdata->pwr_params_cfg.global_en = of_property_read_bool(child,
						"pwr-global-enable");

	ret = of_property_read_u32(child, "pwr-target-temp", &val);
	if (!ret)
		pdata->pwr_params_cfg.target_temp = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(child, "pwr-exit-temp", &val);
	if (!ret)
		pdata->pwr_params_cfg.exit_temp = val | CS35L45_VALID_PDATA;

	of_node_put(child);

bst_bpe_voltage_cfg:
#endif
	child = of_get_child_by_name(node, "cirrus,bst-bpe-voltage-config");
	pdata->bst_bpe_voltage_cfg.is_present = child ? true : false;
	if (!pdata->bst_bpe_voltage_cfg.is_present)
		goto bst_bpe_ind_curr_cfg;

	for (i = 0; i < ARRAY_SIZE(iv_pairs); i++) {
		sprintf(of_name, "bst-bpe-response-l%d", i + 1);
		ret = of_property_read_u32_array(child, of_name, pair, 2);
		if (!ret) {
			iv_pairs[i]->volt = pair[0] | CS35L45_VALID_PDATA;
			iv_pairs[i]->amp = pair[1] | CS35L45_VALID_PDATA;
		}
	}

	of_node_put(child);

bst_bpe_ind_curr_cfg:
	child = of_get_child_by_name(node, "cirrus,bst-bpe-ind-curr-config");
	pdata->bst_bpe_ind_curr_cfg.is_present = child ? true : false;
	if (!pdata->bst_bpe_ind_curr_cfg.is_present)
		goto hvlv_cfg;

	for (i = BST_BPE_IL_LIM_THLD_DEL1; i < BST_BPE_IND_CURR_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_ind_curr_param(cs35l45, i);
		ret = of_property_read_u32(child, bst_bpe_ind_curr_map[i].name,
					   &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

hvlv_cfg:
	child = of_get_child_by_name(node, "cirrus,hvlv-config");
	pdata->hvlv_cfg.is_present = child ? true : false;
	if (!pdata->hvlv_cfg.is_present)
		goto ldpm_cfg;

	ret = of_property_read_u32(child, "hvlv-thld-hys", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_thld_hys = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(child, "hvlv-thld", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_thld = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(child, "hvlv-dly", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_dly = val | CS35L45_VALID_PDATA;

	of_node_put(child);

ldpm_cfg:
	child = of_get_child_by_name(node, "cirrus,ldpm-config");
	pdata->ldpm_cfg.is_present = child ? true : false;
	if (!pdata->ldpm_cfg.is_present)
		goto classh_cfg;

	for (i = LDPM_GP1_BOOST_SEL; i < LDPM_PARAMS; i++) {
		ptr = cs35l45_get_ldpm_param(cs35l45, i);
		ret = of_property_read_u32(child, ldpm_map[i].name, &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

classh_cfg:
	child = of_get_child_by_name(node, "cirrus,classh-config");
	pdata->classh_cfg.is_present = child ? true : false;
	if (!pdata->classh_cfg.is_present)
		goto gpio_cfg;

	for (i = CH_HDRM; i < CLASSH_PARAMS; i++) {
		ptr = cs35l45_get_classh_param(cs35l45, i);
		ret = of_property_read_u32(child, classh_map[i].name, &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

gpio_cfg:
	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		sprintf(of_name, "cirrus,gpio-ctrl%d", i + 1);
		child = of_get_child_by_name(node, of_name);
		gpios[i]->is_present = child ? true : false;
		if (!gpios[i]->is_present)
			continue;

		ret = of_property_read_u32(child, "gpio-dir", &val);
		if (!ret)
			gpios[i]->dir = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-lvl", &val);
		if (!ret)
			gpios[i]->lvl = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-op-cfg", &val);
		if (!ret)
			gpios[i]->op_cfg = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-pol", &val);
		if (!ret)
			gpios[i]->pol = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-ctrl", &val);
		if (!ret)
			gpios[i]->ctrl = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-invert", &val);
		if (!ret)
			gpios[i]->invert = val | CS35L45_VALID_PDATA;

		of_node_put(child);
	}

	return 0;
}

static int cs35l45_hibernate(struct cs35l45_private *cs35l45, bool hiber_en)
{
	struct snd_soc_component *component =
			snd_soc_lookup_component(cs35l45->dev, NULL);
	unsigned int sts, cmd, val;
	int status, ret, i;
	struct cs35l45_mixer_cache mixer_cache[] = {
		{CS35L45_AMP_GAIN, CS35L45_AMP_GAIN_PCM_MASK, 0x00000000},
		{CS35L45_AMP_OUTPUT_MUTE, CS35L45_AMP_MUTE_MASK, 0x00000000},
		{CS35L45_ASPTX1_INPUT,	CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_ASPTX2_INPUT,	CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_ASPTX3_INPUT,	CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_ASPTX4_INPUT,	CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_DSP1RX1_INPUT,	CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_DSP1RX2_INPUT, CS35L45_PCM_SRC_MASK, 0x00000000},
		{CS35L45_DACPCM1_INPUT, CS35L45_PCM_SRC_MASK, 0x00000000},
	};

	if (hiber_en == cs35l45->hibernate_mode)
		return 0;

	status = snd_soc_component_get_pin_status(component, "SPK") |
		 snd_soc_component_get_pin_status(component, "RCV") |
		 snd_soc_component_get_pin_status(component, "AP");
	if (status) {
		dev_info(cs35l45->dev, "Amp is active; cannot hibernate\n");
		return -EPERM;
	}

	if (hiber_en == HIBER_MODE_EN) {
		if (!cs35l45->dsp.booted) {
			dev_err(cs35l45->dev, "Firmware not loaded\n");
			return -EPERM;
		}

		regmap_read(cs35l45->regmap, CS35L45_DSP_MBOX_2, &sts);
		if (((enum cspl_mboxstate)sts) != CSPL_MBOX_STS_PAUSED) {
			dev_err(cs35l45->dev, "FW not paused (%d)\n", sts);
			return -EINVAL;
		}

		cmd = CSPL_MBOX_CMD_HIBERNATE;
		regmap_write(cs35l45->regmap, CS35L45_DSP_VIRT1_MBOX_1, cmd);

		ret = cs35l45_activate_ctl(cs35l45, "DSP1 Preload Switch",
					   false);
		if (ret < 0)
			dev_err(cs35l45->dev, "Unable to deactivate ctl (%d)\n",
				ret);

		cs35l45->initialized = false;

		regcache_cache_only(cs35l45->regmap, true);
	} else  /* HIBER_MODE_DIS */ {
		if (!cs35l45->dsp.booted) {
			dev_err(cs35l45->dev, "Firmware not loaded\n");
			return -EPERM;
		}

		usleep_range(200, 300);

		for (i = 0; i < ARRAY_SIZE(mixer_cache); i++)
			regmap_read(cs35l45->regmap, mixer_cache[i].reg,
				    &mixer_cache[i].val);

		regcache_cache_only(cs35l45->regmap, false);

		regcache_drop_region(cs35l45->regmap, CS35L45_DEVID,
				     CS35L45_MIXER_NGATE_CH2_CFG);

		for (i = 0; i < 5; i++) {
			usleep_range(200, 300);

			ret = regmap_read(cs35l45->regmap, CS35L45_DEVID, &val);
			if (!ret)
				break;
		}

		if (i == 5) {
			dev_info(cs35l45->dev, "Timeout trying to wake amp");
			return -ETIMEDOUT;
		}

		ret = __cs35l45_initialize(cs35l45);
		if (ret < 0) {
			dev_err(cs35l45->dev, "Failed to reinitialize (%d)\n",
				ret);
			return ret;
		}

		regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
				   CS35L45_MEM_RDY_MASK, CS35L45_MEM_RDY_MASK);

		usleep_range(100, 200);

		cmd = CSPL_MBOX_CMD_OUT_OF_HIBERNATE;
		ret = cs35l45_set_csplmboxcmd(cs35l45, cmd);
		if (ret < 0)
			dev_err(cs35l45->dev, "MBOX failure (%d)\n", ret);

		for (i = 0; i < ARRAY_SIZE(mixer_cache); i++)
			regmap_update_bits(cs35l45->regmap, mixer_cache[i].reg,
					   mixer_cache[i].mask,
					   mixer_cache[i].val);

		ret = cs35l45_activate_ctl(cs35l45, "DSP1 Preload Switch",
					   true);
		if (ret < 0)
			dev_err(cs35l45->dev, "Unable to activate ctl (%d)\n",
				ret);
	}

	cs35l45->hibernate_mode = hiber_en;

	return 0;
}

static const struct reg_sequence cs35l45_errata_patch[] = {
	{0x00000040,		0x00000055},
	{0x00000040,		0x000000AA},
	{0x00000044,		0x00000055},
	{0x00000044,		0x000000AA},
	{0x00006480,		0x0830500A},
	{CS35L45_LDPM_CONFIG,	0x0001B636},
	{0x00007C60,		0x1000850B},
	{CS35L45_BOOST_OV_CFG,	0x007000D0},
	{0x00003820,		0x00040100},
	{0x00003824,		0x00000000},
	{0x00000040,		0x00000000},
	{0x00000044,		0x00000000},
};

static int __cs35l45_initialize(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	unsigned int sts;
	int ret, i;

	if (cs35l45->initialized)
		return -EPERM;

	for (i = 0; i < 5; i++) {
		usleep_range(1000, 1100);

		regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_4, &sts);
		if (!(sts & CS35L45_OTP_BOOT_DONE_STS_MASK))
			continue;

		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_4,
			     CS35L45_OTP_BOOT_DONE_STS_MASK |
			     CS35L45_OTP_BUSY_MASK);

		break;
	}

	if (i == 5) {
		dev_err(cs35l45->dev, "Timeout waiting for OTP boot\n");
		return -ETIMEDOUT;
	}

	ret = regmap_register_patch(cs35l45->regmap, cs35l45_errata_patch,
				    ARRAY_SIZE(cs35l45_errata_patch));
	if (ret < 0) {
		dev_err(dev, "Failed to apply errata patch %d\n", ret);
		return ret;
	}

	regmap_write(cs35l45->regmap, CS35L45_MIXER_PILOT0_INPUT,
		     CS35L45_PCM_SRC_DSP_TX2);

	ret = cs35l45_apply_of_data(cs35l45);
	if (ret < 0) {
		dev_err(dev, "applying OF data failed (%d)\n", ret);
		return ret;
	}

	if (cs35l45->irq) {
		ret = cs35l45_register_irq_monitors(cs35l45);
		if (ret < 0) {
			dev_err(dev, "Failed to register IRQ monitors: %d\n",
				ret);
			return ret;
		}
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_WAKESRC_CTL,
			   CS35L45_WKSRC_EN_MASK,
			   cs35l45->wksrc << CS35L45_WKSRC_EN_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_WAKESRC_CTL,
			   CS35L45_UPDT_WKCTL_MASK, CS35L45_UPDT_WKCTL_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_WKI2C_CTL,
			   CS35L45_WKI2C_ADDR_MASK, cs35l45->i2c_addr);

	regmap_update_bits(cs35l45->regmap, CS35L45_WKI2C_CTL,
			   CS35L45_UPDT_WKI2C_MASK, CS35L45_UPDT_WKI2C_MASK);

	cs35l45->initialized = true;

	return 0;
}

int cs35l45_initialize(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	unsigned int dev_id, rev_id;
	int ret;

	ret = regmap_read(cs35l45->regmap, CS35L45_DEVID, &dev_id);
	if (ret < 0) {
		dev_err(dev, "Get Device ID failed\n");
		return ret;
	}

	ret = regmap_read(cs35l45->regmap, CS35L45_REVID, &rev_id);
	if (ret < 0) {
		dev_err(dev, "Get Revision ID failed\n");
		return ret;
	}

	ret = __cs35l45_initialize(cs35l45);
	if (ret < 0) {
		dev_err(dev, "CS35L45 failed to initialize (%d)\n", ret);
		return ret;
	}

	regmap_update_bits(cs35l45->regmap,
			   CS35L45_DSP1_STREAM_ARB_TX1_CONFIG_0,
			   CS35L45_DSP1_STREAM_ARB_TX1_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap,
			   CS35L45_DSP1_STREAM_ARB_MSTR1_CONFIG_0,
			   CS35L45_DSP1_STREAM_ARB_MSTR0_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_DSP1_CCM_CORE_CONTROL,
			   CS35L45_CCM_CORE_EN_MASK, 0);

	dev_info(dev, "Cirrus Logic CS35L45 (%x), Revision: %02X\n", dev_id,
		 rev_id);

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_initialize);

static const struct wm_adsp_region cs35l45_dsp1_regions[] = {
	{ .type = WMFW_HALO_PM_PACKED,	.base = CS35L45_DSP1_PMEM_0 },
	{ .type = WMFW_HALO_XM_PACKED,	.base = CS35L45_DSP1_XMEM_PACK_0 },
	{ .type = WMFW_HALO_YM_PACKED,	.base = CS35L45_DSP1_YMEM_PACK_0 },
	{. type = WMFW_ADSP2_XM,	.base = CS35L45_DSP1_XMEM_UNPACK24_0},
	{. type = WMFW_ADSP2_YM,	.base = CS35L45_DSP1_YMEM_UNPACK24_0},
};

static int cs35l45_dsp_init(struct cs35l45_private *cs35l45)
{
	struct wm_adsp *dsp = &cs35l45->dsp;
	int ret, i;

	dsp->part = cs35l45->pdata.dsp_part_name;
	dsp->num = 1;
	dsp->type = WMFW_HALO;
	dsp->rev = 0;
	dsp->dev = cs35l45->dev;
	dsp->regmap = cs35l45->regmap;
	dsp->base = CS35L45_DSP1_CLOCK_FREQ;
	dsp->base_sysinfo = CS35L45_DSP1_SYS_ID;
	dsp->mem = cs35l45_dsp1_regions;
	dsp->num_mems = ARRAY_SIZE(cs35l45_dsp1_regions);
	dsp->lock_regions = 0xFFFFFFFF;
	dsp->n_rx_channels = CS35L45_DSP_N_RX_RATES;
	dsp->n_tx_channels = CS35L45_DSP_N_TX_RATES;

	mutex_init(&cs35l45->rate_lock);
	ret = wm_halo_init(dsp, &cs35l45->rate_lock);

	for (i = 0; i < CS35L45_DSP_N_RX_RATES; i++)
		dsp->rx_rate_cache[i] = 0x1;

	for (i = 0; i < CS35L45_DSP_N_TX_RATES; i++)
		dsp->tx_rate_cache[i] = 0x1;

	return ret;
}

static const char * const cs35l45_supplies[] = {"VA", "VP"};

int cs35l45_probe(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	unsigned long irq_pol = IRQF_ONESHOT;
	int ret;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(cs35l45_supplies); i++)
		cs35l45->supplies[i].supply = cs35l45_supplies[i];

	ret = devm_regulator_bulk_get(dev, CS35L45_NUM_SUPPLIES,
				      cs35l45->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to request core supplies: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable core supplies: %d\n", ret);
		return ret;
	}

	/* returning NULL can be an option if in stereo mode */
	cs35l45->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						      GPIOD_OUT_LOW);
	if (IS_ERR(cs35l45->reset_gpio)) {
		ret = PTR_ERR(cs35l45->reset_gpio);
		cs35l45->reset_gpio = NULL;
		if (ret == -EBUSY) {
			dev_info(dev,
				 "Reset line busy, assuming shared reset\n");
		} else {
			dev_err(dev, "Failed to get reset GPIO: %d\n", ret);
			goto err;
		}
	}

	if (cs35l45->reset_gpio) {
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 0);
		usleep_range(2000, 2100);
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 1);
	}

	cs35l45->slot_width = CS35L45_DEFAULT_SLOT_WIDTH;

	ret = cs35l45_parse_of_data(cs35l45);
	if (ret < 0) {
		dev_err(dev, "parsing OF data failed: %d\n", ret);
		goto err;
	}

	ret = cs35l45_dsp_init(cs35l45);
	if (ret < 0) {
		dev_err(dev, "dsp_init failed: %d\n", ret);
		goto err;
	}

	if (cs35l45->irq) {
		if (cs35l45->pdata.gpio_ctrl2.invert & (~CS35L45_VALID_PDATA))
			irq_pol |= IRQF_TRIGGER_HIGH;
		else
			irq_pol |= IRQF_TRIGGER_LOW;

		ret = devm_request_threaded_irq(dev, cs35l45->irq, NULL,
						cs35l45_irq, irq_pol,
						"cs35l45", cs35l45);
		if (ret < 0)
			dev_warn(cs35l45->dev, "Failed to request IRQ: %d\n",
				 ret);
	}

	return devm_snd_soc_register_component(dev, &cs35l45_component,
					       &cs35l45_dai, 1);

err:
	regulator_bulk_disable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);
	return ret;
}
EXPORT_SYMBOL_GPL(cs35l45_probe);

int cs35l45_remove(struct cs35l45_private *cs35l45)
{
	if (cs35l45->reset_gpio)
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 0);

	wm_adsp2_remove(&cs35l45->dsp);
	regulator_bulk_disable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_remove);

MODULE_DESCRIPTION("ASoC CS35L45 driver");
MODULE_AUTHOR("James Schulman, Cirrus Logic Inc, <james.schulman@cirrus.com>");
MODULE_LICENSE("GPL");
