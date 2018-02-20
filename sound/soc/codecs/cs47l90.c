/*
 * cs47l90.c  --  ALSA SoC Audio driver for CS47L90 codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * Author: Nikesh Oswal <nikesh@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/registers.h>

#include "madera.h"
#include "wm_adsp.h"

#if IS_ENABLED(CONFIG_SND_SOC_AOV_TRIGGER)
#include "aov_trigger.h"
#endif

#define CS47L90_NUM_ADSP 7
#define ADSP2_CONTROL	0x0
#define ADSP2_CORE_ENA	0x0002

struct cs47l90 {
	struct madera_priv core;
	struct madera_fll fll[3];
};

static const int cs47l90_fx_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_EQ1MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_EQ2MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_EQ3MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_EQ4MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_DRC1LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_DRC1RMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_DRC2LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_DRC2RMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_HPLP1MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_HPLP2MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_HPLP3MIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_HPLP4MIX_INPUT_1_SOURCE, 2),
};

static const int cs47l90_asrc1_1_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ASRC1_1LMIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_asrc1_2_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ASRC1_2LMIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_asrc2_1_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ASRC2_1LMIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_asrc2_2_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ASRC2_2LMIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc1_fsl_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_ISRC1INT1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc1_fsh_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_ISRC1DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc2_fsl_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_ISRC2INT1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc2_fsh_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_ISRC2DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc3_fsl_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ISRC3INT1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc3_fsh_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ISRC3DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc4_fsl_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ISRC4INT1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_isrc4_fsh_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_ISRC4DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_out_inputs[] = {
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT1LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT1RMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT2LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT2RMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT3LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT3RMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT5LMIX_INPUT_1_SOURCE, 2),
	MADERA_MIXER_INPUTS_4_N(MADERA_OUT5RMIX_INPUT_1_SOURCE, 2),
};

static const int cs47l90_spd1_inputs[] = {
	MADERA_MIXER_INPUTS_2_N(MADERA_SPDIF1TX1MIX_INPUT_1_SOURCE, 8),
};

static const int cs47l90_dsp1_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP1LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp2_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP2LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp3_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP3LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp4_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP4LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp5_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP5LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp6_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP6LMIX_INPUT_1_SOURCE),
};

static const int cs47l90_dsp7_inputs[] = {
	MADERA_DSP_MIXER_INPUTS(MADERA_DSP7LMIX_INPUT_1_SOURCE),
};

static const struct wm_adsp_region cs47l90_dsp1_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x080000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x0e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x0a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x0c0000 },
};

static const struct wm_adsp_region cs47l90_dsp2_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x100000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x160000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x120000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x140000 },
};

static const struct wm_adsp_region cs47l90_dsp3_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x180000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x1e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x1a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x1c0000 },
};

static const struct wm_adsp_region cs47l90_dsp4_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x200000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x260000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x220000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x240000 },
};

static const struct wm_adsp_region cs47l90_dsp5_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x280000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x2e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x2a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x2c0000 },
};

static const struct wm_adsp_region cs47l90_dsp6_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x300000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x360000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x320000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x340000 },
};

static const struct wm_adsp_region cs47l90_dsp7_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x380000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x3e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x3a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x3c0000 },
};

static const struct wm_adsp_region *cs47l90_dsp_regions[] = {
	cs47l90_dsp1_regions,
	cs47l90_dsp2_regions,
	cs47l90_dsp3_regions,
	cs47l90_dsp4_regions,
	cs47l90_dsp5_regions,
	cs47l90_dsp6_regions,
	cs47l90_dsp7_regions,
};

static const int cs47l90_dsp_control_bases[] = {
	MADERA_DSP1_CONFIG_1,
	MADERA_DSP2_CONFIG_1,
	MADERA_DSP3_CONFIG_1,
	MADERA_DSP4_CONFIG_1,
	MADERA_DSP5_CONFIG_1,
	MADERA_DSP6_CONFIG_1,
	MADERA_DSP7_CONFIG_1,
};

static int cs47l90_get_sources(unsigned int reg,
				const unsigned int **cur_sources, int *lim)
{
	int ret = 0;

	switch (reg) {
	case MADERA_FX_CTRL1:
		*cur_sources = cs47l90_fx_inputs;
		*lim = ARRAY_SIZE(cs47l90_fx_inputs);
		break;
	case MADERA_ASRC1_RATE1:
		*cur_sources = cs47l90_asrc1_1_inputs;
		*lim = ARRAY_SIZE(cs47l90_asrc1_1_inputs);
		break;
	case MADERA_ASRC1_RATE2:
		*cur_sources = cs47l90_asrc1_2_inputs;
		*lim = ARRAY_SIZE(cs47l90_asrc1_2_inputs);
		break;
	case MADERA_ASRC2_RATE1:
		*cur_sources = cs47l90_asrc2_1_inputs;
		*lim = ARRAY_SIZE(cs47l90_asrc2_1_inputs);
		break;
	case MADERA_ASRC2_RATE2:
		*cur_sources = cs47l90_asrc2_2_inputs;
		*lim = ARRAY_SIZE(cs47l90_asrc2_2_inputs);
		break;
	case MADERA_ISRC_1_CTRL_1:
		*cur_sources = cs47l90_isrc1_fsh_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc1_fsh_inputs);
		break;
	case MADERA_ISRC_1_CTRL_2:
		*cur_sources = cs47l90_isrc1_fsl_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc1_fsl_inputs);
		break;
	case MADERA_ISRC_2_CTRL_1:
		*cur_sources = cs47l90_isrc2_fsh_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc2_fsh_inputs);
		break;
	case MADERA_ISRC_2_CTRL_2:
		*cur_sources = cs47l90_isrc2_fsl_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc2_fsl_inputs);
		break;
	case MADERA_ISRC_3_CTRL_1:
		*cur_sources = cs47l90_isrc3_fsh_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc3_fsh_inputs);
		break;
	case MADERA_ISRC_3_CTRL_2:
		*cur_sources = cs47l90_isrc3_fsl_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc3_fsl_inputs);
		break;
	case MADERA_ISRC_4_CTRL_1:
		*cur_sources = cs47l90_isrc4_fsh_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc4_fsh_inputs);
		break;
	case MADERA_ISRC_4_CTRL_2:
		*cur_sources = cs47l90_isrc4_fsl_inputs;
		*lim = ARRAY_SIZE(cs47l90_isrc4_fsl_inputs);
		break;
	case MADERA_OUTPUT_RATE_1:
		*cur_sources = cs47l90_out_inputs;
		*lim = ARRAY_SIZE(cs47l90_out_inputs);
		break;
	case MADERA_SPD1_TX_CONTROL:
		*cur_sources = cs47l90_spd1_inputs;
		*lim = ARRAY_SIZE(cs47l90_spd1_inputs);
		break;
	case MADERA_DSP1_CONFIG_1:
		*cur_sources = cs47l90_dsp1_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp1_inputs);
		break;
	case MADERA_DSP2_CONFIG_1:
		*cur_sources = cs47l90_dsp2_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp2_inputs);
		break;
	case MADERA_DSP3_CONFIG_1:
		*cur_sources = cs47l90_dsp3_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp3_inputs);
		break;
	case MADERA_DSP4_CONFIG_1:
		*cur_sources = cs47l90_dsp4_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp4_inputs);
		break;
	case MADERA_DSP5_CONFIG_1:
		*cur_sources = cs47l90_dsp5_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp5_inputs);
		break;
	case MADERA_DSP6_CONFIG_1:
		*cur_sources = cs47l90_dsp6_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp6_inputs);
		break;
	case MADERA_DSP7_CONFIG_1:
		*cur_sources = cs47l90_dsp7_inputs;
		*lim = ARRAY_SIZE(cs47l90_dsp7_inputs);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int cs47l90_adsp_power_ev(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);
	struct madera_priv *priv = &cs47l90->core;
	struct madera *madera = priv->madera;
	unsigned int freq;
	int ret;

	ret = regmap_read(madera->regmap, MADERA_DSP_CLOCK_2, &freq);
	if (ret != 0) {
		dev_err(madera->dev,
			"Failed to read MADERA_DSP_CLOCK_2: %d\n", ret);
		return ret;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = madera_set_adsp_clk(&cs47l90->core.adsp[w->shift], freq);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return wm_adsp2_early_event(w, kcontrol, event, freq);
}

static int cs47l90_get_dsp_state(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)
		kcontrol->private_value;
	struct wm_adsp *dsp = &dsps[mc->shift];
	unsigned int val;

	regmap_read(dsp->regmap, dsp->base + ADSP2_CONTROL, &val);
	if (val & ADSP2_CORE_ENA)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int cs47l90_put_dsp_state(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int cs47l90_get_trig_state(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);
	struct madera_priv *priv = &cs47l90->core;
	/* DSP3, Channel 1 */
	struct wm_adsp_compr *compr = priv->adsp[2].compr[0];
	if (compr)
		ucontrol->value.integer.value[0] = compr->freed;
	return 0;
}

static int cs47l90_put_trig_state(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);
	struct madera_priv *priv = &cs47l90->core;
	/* DSP3, Channel 1 */
	struct wm_adsp_compr *compr = priv->adsp[2].compr[0];
	int value = ucontrol->value.integer.value[0];

	if (compr)
		compr->freed = value;
	return 0;
}

#define CS47L90_NG_SRC(name, base) \
	SOC_SINGLE(name " NG HPOUT1L Switch",  base,  0, 1, 0), \
	SOC_SINGLE(name " NG HPOUT1R Switch",  base,  1, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2L Switch",  base,  2, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2R Switch",  base,  3, 1, 0), \
	SOC_SINGLE(name " NG HPOUT3L Switch",  base,  4, 1, 0), \
	SOC_SINGLE(name " NG HPOUT3R Switch",  base,  5, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1L Switch", base,  8, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1R Switch", base,  9, 1, 0)

#define CS47L90_RXANC_INPUT_ROUTES(widget, name) \
	{ widget, NULL, name " NG Mux" }, \
	{ name " NG Internal", NULL, "RXANC NG Clock" }, \
	{ name " NG Internal", NULL, name " Channel" }, \
	{ name " NG External", NULL, "RXANC NG External Clock" }, \
	{ name " NG External", NULL, name " Channel" }, \
	{ name " NG Mux", "None", name " Channel" }, \
	{ name " NG Mux", "Internal", name " NG Internal" }, \
	{ name " NG Mux", "External", name " NG External" }, \
	{ name " Channel", "Left", name " Left Input" }, \
	{ name " Channel", "Combine", name " Left Input" }, \
	{ name " Channel", "Right", name " Right Input" }, \
	{ name " Channel", "Combine", name " Right Input" }, \
	{ name " Left Input", "IN1", "IN1L PGA" }, \
	{ name " Right Input", "IN1", "IN1R PGA" }, \
	{ name " Left Input", "IN2", "IN2L PGA" }, \
	{ name " Right Input", "IN2", "IN2R PGA" }, \
	{ name " Left Input", "IN3", "IN3L PGA" }, \
	{ name " Right Input", "IN3", "IN3R PGA" }, \
	{ name " Left Input", "IN4", "IN4L PGA" }, \
	{ name " Right Input", "IN4", "IN4R PGA" }, \
	{ name " Left Input", "IN5", "IN5L PGA" }, \
	{ name " Right Input", "IN5", "IN5R PGA" }

#define CS47L90_RXANC_OUTPUT_ROUTES(widget, name) \
	{ widget, NULL, name " ANC Source" }, \
	{ name " ANC Source", "RXANCL", "RXANCL" }, \
	{ name " ANC Source", "RXANCR", "RXANCR" }

static const struct snd_kcontrol_new cs47l90_snd_controls[] = {
SOC_ENUM("IN1 OSR", madera_in_dmic_osr[0]),
SOC_ENUM("IN2 OSR", madera_in_dmic_osr[1]),
SOC_ENUM("IN3 OSR", madera_in_dmic_osr[2]),
SOC_ENUM("IN4 OSR", madera_in_dmic_osr[3]),
SOC_ENUM("IN5 OSR", madera_in_dmic_osr[4]),

SOC_SINGLE_RANGE_TLV("IN1L Volume", MADERA_IN1L_CONTROL,
		     MADERA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, madera_ana_tlv),
SOC_SINGLE_RANGE_TLV("IN1R Volume", MADERA_IN1R_CONTROL,
		     MADERA_IN1R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, madera_ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2L Volume", MADERA_IN2L_CONTROL,
		     MADERA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, madera_ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2R Volume", MADERA_IN2R_CONTROL,
		     MADERA_IN2R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, madera_ana_tlv),

SOC_ENUM("IN HPF Cutoff Frequency", madera_in_hpf_cut_enum),

SOC_SINGLE_EXT("IN1L LP Switch", MADERA_ADC_DIGITAL_VOLUME_1L,
		MADERA_IN1L_LP_MODE_SHIFT, 1, 0,
		snd_soc_get_volsw, madera_lp_mode_put),
SOC_SINGLE_EXT("IN1R LP Switch", MADERA_ADC_DIGITAL_VOLUME_1R,
		MADERA_IN1R_LP_MODE_SHIFT, 1, 0,
		snd_soc_get_volsw, madera_lp_mode_put),
SOC_SINGLE_EXT("IN2L LP Switch", MADERA_ADC_DIGITAL_VOLUME_2L,
		MADERA_IN2L_LP_MODE_SHIFT, 1, 0,
		snd_soc_get_volsw, madera_lp_mode_put),
SOC_SINGLE_EXT("IN2R LP Switch", MADERA_ADC_DIGITAL_VOLUME_2R,
		MADERA_IN2R_LP_MODE_SHIFT, 1, 0,
		snd_soc_get_volsw, madera_lp_mode_put),

SOC_SINGLE("IN1L HPF Switch", MADERA_IN1L_CONTROL,
	   MADERA_IN1L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN1R HPF Switch", MADERA_IN1R_CONTROL,
	   MADERA_IN1R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2L HPF Switch", MADERA_IN2L_CONTROL,
	   MADERA_IN2L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2R HPF Switch", MADERA_IN2R_CONTROL,
	   MADERA_IN2R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN3L HPF Switch", MADERA_IN3L_CONTROL,
	   MADERA_IN3L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN3R HPF Switch", MADERA_IN3R_CONTROL,
	   MADERA_IN3R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN4L HPF Switch", MADERA_IN4L_CONTROL,
	   MADERA_IN4L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN4R HPF Switch", MADERA_IN4R_CONTROL,
	   MADERA_IN4R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN5L HPF Switch", MADERA_IN5L_CONTROL,
	   MADERA_IN5L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN5R HPF Switch", MADERA_IN5R_CONTROL,
	   MADERA_IN5R_HPF_SHIFT, 1, 0),

SOC_SINGLE_EXT("Set Trigger State Switch", SND_SOC_NOPM, 0, 1, 0,
	       cs47l90_get_trig_state,
	       cs47l90_put_trig_state),
SOC_SINGLE_EXT("Get DSP1 State", SND_SOC_NOPM, 0, 1, 0, cs47l90_get_dsp_state,
	       cs47l90_put_dsp_state),
SOC_SINGLE_EXT("Get DSP2 State", SND_SOC_NOPM, 1, 1, 0, cs47l90_get_dsp_state,
	       cs47l90_put_dsp_state),
SOC_SINGLE_EXT("Get DSP3 State", SND_SOC_NOPM, 2, 1, 0, cs47l90_get_dsp_state,
	       cs47l90_put_dsp_state),

SOC_SINGLE_TLV("IN1L Digital Volume", MADERA_ADC_DIGITAL_VOLUME_1L,
	       MADERA_IN1L_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN1R Digital Volume", MADERA_ADC_DIGITAL_VOLUME_1R,
	       MADERA_IN1R_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN2L Digital Volume", MADERA_ADC_DIGITAL_VOLUME_2L,
	       MADERA_IN2L_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN2R Digital Volume", MADERA_ADC_DIGITAL_VOLUME_2R,
	       MADERA_IN2R_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN3L Digital Volume", MADERA_ADC_DIGITAL_VOLUME_3L,
	       MADERA_IN3L_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN3R Digital Volume", MADERA_ADC_DIGITAL_VOLUME_3R,
	       MADERA_IN3R_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN4L Digital Volume", MADERA_ADC_DIGITAL_VOLUME_4L,
	       MADERA_IN4L_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN4R Digital Volume", MADERA_ADC_DIGITAL_VOLUME_4R,
	       MADERA_IN4R_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN5L Digital Volume", MADERA_ADC_DIGITAL_VOLUME_5L,
	       MADERA_IN5L_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),
SOC_SINGLE_TLV("IN5R Digital Volume", MADERA_ADC_DIGITAL_VOLUME_5R,
	       MADERA_IN5R_DIG_VOL_SHIFT, 0xbf, 0, madera_digital_tlv),

SOC_ENUM("Input Ramp Up", madera_in_vi_ramp),
SOC_ENUM("Input Ramp Down", madera_in_vd_ramp),

SND_SOC_BYTES("RXANC Coefficients", MADERA_ANC_COEFF_START,
	      MADERA_ANC_COEFF_END - MADERA_ANC_COEFF_START + 1),
SND_SOC_BYTES("RXANCL Config", MADERA_FCL_FILTER_CONTROL, 1),
SND_SOC_BYTES("RXANCL Coefficients", MADERA_FCL_COEFF_START,
	      MADERA_FCL_COEFF_END - MADERA_FCL_COEFF_START + 1),
SND_SOC_BYTES("RXANCR Config", MADERA_FCR_FILTER_CONTROL, 1),
SND_SOC_BYTES("RXANCR Coefficients", MADERA_FCR_COEFF_START,
	      MADERA_FCR_COEFF_END - MADERA_FCR_COEFF_START + 1),

MADERA_FRF_BYTES("FRF COEFF 1L", MADERA_FRF_COEFFICIENT_1L_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 1R", MADERA_FRF_COEFFICIENT_1R_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 2L", MADERA_FRF_COEFFICIENT_2L_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 2R", MADERA_FRF_COEFFICIENT_2R_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 3L", MADERA_FRF_COEFFICIENT_3L_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 3R", MADERA_FRF_COEFFICIENT_3R_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 5L", MADERA_FRF_COEFFICIENT_5L_1,
				 MADERA_FRF_COEFFICIENT_LEN),
MADERA_FRF_BYTES("FRF COEFF 5R", MADERA_FRF_COEFFICIENT_5R_1,
				 MADERA_FRF_COEFFICIENT_LEN),

SND_SOC_BYTES("DAC COMP 1", MADERA_DAC_COMP_1, 1),
SND_SOC_BYTES("DAC COMP 2", MADERA_DAC_COMP_2, 1),

MADERA_MIXER_CONTROLS("EQ1", MADERA_EQ1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("EQ2", MADERA_EQ2MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("EQ3", MADERA_EQ3MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("EQ4", MADERA_EQ4MIX_INPUT_1_SOURCE),

MADERA_EQ_CONTROL("EQ1 Coefficients", MADERA_EQ1_2),
SOC_SINGLE_TLV("EQ1 B1 Volume", MADERA_EQ1_1, MADERA_EQ1_B1_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ1 B2 Volume", MADERA_EQ1_1, MADERA_EQ1_B2_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ1 B3 Volume", MADERA_EQ1_1, MADERA_EQ1_B3_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ1 B4 Volume", MADERA_EQ1_2, MADERA_EQ1_B4_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ1 B5 Volume", MADERA_EQ1_2, MADERA_EQ1_B5_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),

MADERA_EQ_CONTROL("EQ2 Coefficients", MADERA_EQ2_2),
SOC_SINGLE_TLV("EQ2 B1 Volume", MADERA_EQ2_1, MADERA_EQ2_B1_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ2 B2 Volume", MADERA_EQ2_1, MADERA_EQ2_B2_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ2 B3 Volume", MADERA_EQ2_1, MADERA_EQ2_B3_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ2 B4 Volume", MADERA_EQ2_2, MADERA_EQ2_B4_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ2 B5 Volume", MADERA_EQ2_2, MADERA_EQ2_B5_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),

MADERA_EQ_CONTROL("EQ3 Coefficients", MADERA_EQ3_2),
SOC_SINGLE_TLV("EQ3 B1 Volume", MADERA_EQ3_1, MADERA_EQ3_B1_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ3 B2 Volume", MADERA_EQ3_1, MADERA_EQ3_B2_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ3 B3 Volume", MADERA_EQ3_1, MADERA_EQ3_B3_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ3 B4 Volume", MADERA_EQ3_2, MADERA_EQ3_B4_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ3 B5 Volume", MADERA_EQ3_2, MADERA_EQ3_B5_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),

MADERA_EQ_CONTROL("EQ4 Coefficients", MADERA_EQ4_2),
SOC_SINGLE_TLV("EQ4 B1 Volume", MADERA_EQ4_1, MADERA_EQ4_B1_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ4 B2 Volume", MADERA_EQ4_1, MADERA_EQ4_B2_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ4 B3 Volume", MADERA_EQ4_1, MADERA_EQ4_B3_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ4 B4 Volume", MADERA_EQ4_2, MADERA_EQ4_B4_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),
SOC_SINGLE_TLV("EQ4 B5 Volume", MADERA_EQ4_2, MADERA_EQ4_B5_GAIN_SHIFT,
	       24, 0, madera_eq_tlv),

MADERA_MIXER_CONTROLS("DRC1L", MADERA_DRC1LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DRC1R", MADERA_DRC1RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DRC2L", MADERA_DRC2LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DRC2R", MADERA_DRC2RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC1", MADERA_DRC1_CTRL1, 5,
		   MADERA_DRC1R_ENA | MADERA_DRC1L_ENA),
SND_SOC_BYTES_MASK("DRC2", MADERA_DRC2_CTRL1, 5,
		   MADERA_DRC2R_ENA | MADERA_DRC2L_ENA),

MADERA_MIXER_CONTROLS("LHPF1", MADERA_HPLP1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("LHPF2", MADERA_HPLP2MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("LHPF3", MADERA_HPLP3MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("LHPF4", MADERA_HPLP4MIX_INPUT_1_SOURCE),

MADERA_LHPF_CONTROL("LHPF1 Coefficients", MADERA_HPLPF1_2),
MADERA_LHPF_CONTROL("LHPF2 Coefficients", MADERA_HPLPF2_2),
MADERA_LHPF_CONTROL("LHPF3 Coefficients", MADERA_HPLPF3_2),
MADERA_LHPF_CONTROL("LHPF4 Coefficients", MADERA_HPLPF4_2),

SOC_ENUM("LHPF1 Mode", madera_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", madera_lhpf2_mode),
SOC_ENUM("LHPF3 Mode", madera_lhpf3_mode),
SOC_ENUM("LHPF4 Mode", madera_lhpf4_mode),

SOC_ENUM("Sample Rate 2", madera_sample_rate[0]),
SOC_ENUM("Sample Rate 3", madera_sample_rate[1]),
SOC_ENUM("ASYNC Sample Rate 2", madera_sample_rate[2]),

MADERA_RATE_ENUM("FX Rate", madera_fx_rate),

MADERA_RATE_ENUM("ISRC1 FSL", madera_isrc_fsl[0]),
MADERA_RATE_ENUM("ISRC2 FSL", madera_isrc_fsl[1]),
MADERA_RATE_ENUM("ISRC3 FSL", madera_isrc_fsl[2]),
MADERA_RATE_ENUM("ISRC4 FSL", madera_isrc_fsl[3]),
MADERA_RATE_ENUM("ISRC1 FSH", madera_isrc_fsh[0]),
MADERA_RATE_ENUM("ISRC2 FSH", madera_isrc_fsh[1]),
MADERA_RATE_ENUM("ISRC3 FSH", madera_isrc_fsh[2]),
MADERA_RATE_ENUM("ISRC4 FSH", madera_isrc_fsh[3]),
MADERA_RATE_ENUM("ASRC1 Rate 1", madera_asrc1_rate[0]),
MADERA_RATE_ENUM("ASRC1 Rate 2", madera_asrc1_rate[1]),
MADERA_RATE_ENUM("ASRC2 Rate 1", madera_asrc2_rate[0]),
MADERA_RATE_ENUM("ASRC2 Rate 2", madera_asrc2_rate[1]),

WM_ADSP2_PRELOAD_SWITCH("DSP1", 1),
WM_ADSP2_PRELOAD_SWITCH("DSP2", 2),
WM_ADSP2_PRELOAD_SWITCH("DSP3", 3),
WM_ADSP2_PRELOAD_SWITCH("DSP4", 4),
WM_ADSP2_PRELOAD_SWITCH("DSP5", 5),
WM_ADSP2_PRELOAD_SWITCH("DSP6", 6),
WM_ADSP2_PRELOAD_SWITCH("DSP7", 7),

MADERA_MIXER_CONTROLS("DSP1L", MADERA_DSP1LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP1R", MADERA_DSP1RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP2L", MADERA_DSP2LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP2R", MADERA_DSP2RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP3L", MADERA_DSP3LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP3R", MADERA_DSP3RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP4L", MADERA_DSP4LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP4R", MADERA_DSP4RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP5L", MADERA_DSP5LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP5R", MADERA_DSP5RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP6L", MADERA_DSP6LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP6R", MADERA_DSP6RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP7L", MADERA_DSP7LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("DSP7R", MADERA_DSP7RMIX_INPUT_1_SOURCE),

SOC_SINGLE_TLV("Noise Generator Volume", MADERA_COMFORT_NOISE_GENERATOR,
	       MADERA_NOISE_GEN_GAIN_SHIFT, 0x16, 0, madera_noise_tlv),

MADERA_MIXER_CONTROLS("HPOUT1L", MADERA_OUT1LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("HPOUT1R", MADERA_OUT1RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("HPOUT2L", MADERA_OUT2LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("HPOUT2R", MADERA_OUT2RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("HPOUT3L", MADERA_OUT3LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("HPOUT3R", MADERA_OUT3RMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SPKDAT1L", MADERA_OUT5LMIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SPKDAT1R", MADERA_OUT5RMIX_INPUT_1_SOURCE),

SOC_SINGLE("HPOUT1 SC Protect Switch", MADERA_HP1_SHORT_CIRCUIT_CTRL,
	   MADERA_HP1_SC_ENA_SHIFT, 1, 0),
SOC_SINGLE("HPOUT2 SC Protect Switch", MADERA_HP2_SHORT_CIRCUIT_CTRL,
	   MADERA_HP2_SC_ENA_SHIFT, 1, 0),
SOC_SINGLE("HPOUT3 SC Protect Switch", MADERA_HP3_SHORT_CIRCUIT_CTRL,
	   MADERA_HP3_SC_ENA_SHIFT, 1, 0),

SOC_SINGLE("SPKDAT1 High Performance Switch", MADERA_OUTPUT_PATH_CONFIG_5L,
	   MADERA_OUT5_OSR_SHIFT, 1, 0),

SOC_DOUBLE_R("HPOUT1 Digital Switch", MADERA_DAC_DIGITAL_VOLUME_1L,
	     MADERA_DAC_DIGITAL_VOLUME_1R, MADERA_OUT1L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT2 Digital Switch", MADERA_DAC_DIGITAL_VOLUME_2L,
	     MADERA_DAC_DIGITAL_VOLUME_2R, MADERA_OUT2L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT3 Digital Switch", MADERA_DAC_DIGITAL_VOLUME_3L,
	     MADERA_DAC_DIGITAL_VOLUME_3R, MADERA_OUT3L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("SPKDAT1 Digital Switch", MADERA_DAC_DIGITAL_VOLUME_5L,
	     MADERA_DAC_DIGITAL_VOLUME_5R, MADERA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("HPOUT1 Digital Volume", MADERA_DAC_DIGITAL_VOLUME_1L,
		 MADERA_DAC_DIGITAL_VOLUME_1R, MADERA_OUT1L_VOL_SHIFT,
		 0xbf, 0, madera_digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT2 Digital Volume", MADERA_DAC_DIGITAL_VOLUME_2L,
		 MADERA_DAC_DIGITAL_VOLUME_2R, MADERA_OUT2L_VOL_SHIFT,
		 0xbf, 0, madera_digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT3 Digital Volume", MADERA_DAC_DIGITAL_VOLUME_3L,
		 MADERA_DAC_DIGITAL_VOLUME_3R, MADERA_OUT3L_VOL_SHIFT,
		 0xbf, 0, madera_digital_tlv),
SOC_DOUBLE_R_TLV("SPKDAT1 Digital Volume", MADERA_DAC_DIGITAL_VOLUME_5L,
		 MADERA_DAC_DIGITAL_VOLUME_5R, MADERA_OUT5L_VOL_SHIFT,
		 0xbf, 0, madera_digital_tlv),

SOC_DOUBLE("SPKDAT1 Switch", MADERA_PDM_SPK1_CTRL_1, MADERA_SPK1L_MUTE_SHIFT,
	   MADERA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_EXT("HPOUT1 DRE Switch", MADERA_DRE_ENABLE,
	   MADERA_DRE1L_ENA_SHIFT, MADERA_DRE1R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, madera_put_dre),
SOC_DOUBLE_EXT("HPOUT2 DRE Switch", MADERA_DRE_ENABLE,
	   MADERA_DRE2L_ENA_SHIFT, MADERA_DRE2R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, madera_put_dre),
SOC_DOUBLE_EXT("HPOUT3 DRE Switch", MADERA_DRE_ENABLE,
	   MADERA_DRE3L_ENA_SHIFT, MADERA_DRE3R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, madera_put_dre),

SOC_DOUBLE("HPOUT1 EDRE Switch", MADERA_EDRE_ENABLE,
	   MADERA_EDRE_OUT1L_THR1_ENA_SHIFT,
	   MADERA_EDRE_OUT1R_THR1_ENA_SHIFT, 1, 0),
SOC_DOUBLE("HPOUT2 EDRE Switch", MADERA_EDRE_ENABLE,
	   MADERA_EDRE_OUT2L_THR1_ENA_SHIFT,
	   MADERA_EDRE_OUT2R_THR1_ENA_SHIFT, 1, 0),
SOC_DOUBLE("HPOUT3 EDRE Switch", MADERA_EDRE_ENABLE,
	   MADERA_EDRE_OUT3L_THR1_ENA_SHIFT,
	   MADERA_EDRE_OUT3R_THR1_ENA_SHIFT, 1, 0),

SOC_ENUM("Output Ramp Up", madera_out_vi_ramp),
SOC_ENUM("Output Ramp Down", madera_out_vd_ramp),

MADERA_RATE_ENUM("SPDIF1 Rate", madera_spdif_rate),

SOC_SINGLE("Noise Gate Switch", MADERA_NOISE_GATE_CONTROL,
	   MADERA_NGATE_ENA_SHIFT, 1, 0),
SOC_SINGLE_TLV("Noise Gate Threshold Volume", MADERA_NOISE_GATE_CONTROL,
	       MADERA_NGATE_THR_SHIFT, 7, 1, madera_ng_tlv),
SOC_ENUM("Noise Gate Hold", madera_ng_hold),

MADERA_RATE_ENUM("Output Rate 1", madera_output_rate),

SOC_ENUM_EXT("IN1L Rate", madera_input_rate[0],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN1R Rate", madera_input_rate[1],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN2L Rate", madera_input_rate[2],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN2R Rate", madera_input_rate[3],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN3L Rate", madera_input_rate[4],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN3R Rate", madera_input_rate[5],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN4L Rate", madera_input_rate[6],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN4R Rate", madera_input_rate[7],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN5L Rate", madera_input_rate[8],
	     snd_soc_get_enum_double, madera_in_rate_put),
SOC_ENUM_EXT("IN5R Rate", madera_input_rate[9],
	     snd_soc_get_enum_double, madera_in_rate_put),

SOC_ENUM_EXT("DFC1RX Width", madera_dfc_width[0],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC1RX Type", madera_dfc_type[0],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC1TX Width", madera_dfc_width[1],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC1TX Type", madera_dfc_type[1],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC2RX Width", madera_dfc_width[2],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC2RX Type", madera_dfc_type[2],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC2TX Width", madera_dfc_width[3],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC2TX Type", madera_dfc_type[3],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC3RX Width", madera_dfc_width[4],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC3RX Type", madera_dfc_type[4],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC3TX Width", madera_dfc_width[5],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC3TX Type", madera_dfc_type[5],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC4RX Width", madera_dfc_width[6],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC4RX Type", madera_dfc_type[6],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC4TX Width", madera_dfc_width[7],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC4TX Type", madera_dfc_type[7],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC5RX Width", madera_dfc_width[8],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC5RX Type", madera_dfc_type[8],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC5TX Width", madera_dfc_width[9],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC5TX Type", madera_dfc_type[9],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC6RX Width", madera_dfc_width[10],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC6RX Type", madera_dfc_type[10],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC6TX Width", madera_dfc_width[11],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC6TX Type", madera_dfc_type[11],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC7RX Width", madera_dfc_width[12],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC7RX Type", madera_dfc_type[12],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC7TX Width", madera_dfc_width[13],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC7TX Type", madera_dfc_type[13],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC8RX Width", madera_dfc_width[14],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC8RX Type", madera_dfc_type[14],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC8TX Width", madera_dfc_width[15],
	     snd_soc_get_enum_double, madera_dfc_put),
SOC_ENUM_EXT("DFC8TX Type", madera_dfc_type[15],
	     snd_soc_get_enum_double, madera_dfc_put),

CS47L90_NG_SRC("HPOUT1L", MADERA_NOISE_GATE_SELECT_1L),
CS47L90_NG_SRC("HPOUT1R", MADERA_NOISE_GATE_SELECT_1R),
CS47L90_NG_SRC("HPOUT2L", MADERA_NOISE_GATE_SELECT_2L),
CS47L90_NG_SRC("HPOUT2R", MADERA_NOISE_GATE_SELECT_2R),
CS47L90_NG_SRC("HPOUT3L", MADERA_NOISE_GATE_SELECT_3L),
CS47L90_NG_SRC("HPOUT3R", MADERA_NOISE_GATE_SELECT_3R),
CS47L90_NG_SRC("SPKDAT1L", MADERA_NOISE_GATE_SELECT_5L),
CS47L90_NG_SRC("SPKDAT1R", MADERA_NOISE_GATE_SELECT_5R),

MADERA_MIXER_CONTROLS("AIF1TX1", MADERA_AIF1TX1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX2", MADERA_AIF1TX2MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX3", MADERA_AIF1TX3MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX4", MADERA_AIF1TX4MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX5", MADERA_AIF1TX5MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX6", MADERA_AIF1TX6MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX7", MADERA_AIF1TX7MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF1TX8", MADERA_AIF1TX8MIX_INPUT_1_SOURCE),

MADERA_MIXER_CONTROLS("AIF2TX1", MADERA_AIF2TX1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX2", MADERA_AIF2TX2MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX3", MADERA_AIF2TX3MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX4", MADERA_AIF2TX4MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX5", MADERA_AIF2TX5MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX6", MADERA_AIF2TX6MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX7", MADERA_AIF2TX7MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF2TX8", MADERA_AIF2TX8MIX_INPUT_1_SOURCE),

MADERA_MIXER_CONTROLS("AIF3TX1", MADERA_AIF3TX1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF3TX2", MADERA_AIF3TX2MIX_INPUT_1_SOURCE),

MADERA_MIXER_CONTROLS("AIF4TX1", MADERA_AIF4TX1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("AIF4TX2", MADERA_AIF4TX2MIX_INPUT_1_SOURCE),

MADERA_MIXER_CONTROLS("SLIMTX1", MADERA_SLIMTX1MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX2", MADERA_SLIMTX2MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX3", MADERA_SLIMTX3MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX4", MADERA_SLIMTX4MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX5", MADERA_SLIMTX5MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX6", MADERA_SLIMTX6MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX7", MADERA_SLIMTX7MIX_INPUT_1_SOURCE),
MADERA_MIXER_CONTROLS("SLIMTX8", MADERA_SLIMTX8MIX_INPUT_1_SOURCE),

MADERA_GAINMUX_CONTROLS("SPDIF1TX1", MADERA_SPDIF1TX1MIX_INPUT_1_SOURCE),
MADERA_GAINMUX_CONTROLS("SPDIF1TX2", MADERA_SPDIF1TX2MIX_INPUT_1_SOURCE),
};

MADERA_MIXER_ENUMS(EQ1, MADERA_EQ1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(EQ2, MADERA_EQ2MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(EQ3, MADERA_EQ3MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(EQ4, MADERA_EQ4MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DRC1L, MADERA_DRC1LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DRC1R, MADERA_DRC1RMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DRC2L, MADERA_DRC2LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DRC2R, MADERA_DRC2RMIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(LHPF1, MADERA_HPLP1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(LHPF2, MADERA_HPLP2MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(LHPF3, MADERA_HPLP3MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(LHPF4, MADERA_HPLP4MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP1L, MADERA_DSP1LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP1R, MADERA_DSP1RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP1, MADERA_DSP1AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP2L, MADERA_DSP2LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP2R, MADERA_DSP2RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP2, MADERA_DSP2AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP3L, MADERA_DSP3LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP3R, MADERA_DSP3RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP3, MADERA_DSP3AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP4L, MADERA_DSP4LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP4R, MADERA_DSP4RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP4, MADERA_DSP4AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP5L, MADERA_DSP5LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP5R, MADERA_DSP5RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP5, MADERA_DSP5AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP6L, MADERA_DSP6LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP6R, MADERA_DSP6RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP6, MADERA_DSP6AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(DSP7L, MADERA_DSP7LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(DSP7R, MADERA_DSP7RMIX_INPUT_1_SOURCE);
MADERA_DSP_AUX_ENUMS(DSP7, MADERA_DSP7AUX1MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(PWM1, MADERA_PWM1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(PWM2, MADERA_PWM2MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(OUT1L, MADERA_OUT1LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(OUT1R, MADERA_OUT1RMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(OUT2L, MADERA_OUT2LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(OUT2R, MADERA_OUT2RMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(OUT3L, MADERA_OUT3LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(OUT3R, MADERA_OUT3RMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SPKDAT1L, MADERA_OUT5LMIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SPKDAT1R, MADERA_OUT5RMIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(AIF1TX1, MADERA_AIF1TX1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX2, MADERA_AIF1TX2MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX3, MADERA_AIF1TX3MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX4, MADERA_AIF1TX4MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX5, MADERA_AIF1TX5MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX6, MADERA_AIF1TX6MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX7, MADERA_AIF1TX7MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF1TX8, MADERA_AIF1TX8MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(AIF2TX1, MADERA_AIF2TX1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX2, MADERA_AIF2TX2MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX3, MADERA_AIF2TX3MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX4, MADERA_AIF2TX4MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX5, MADERA_AIF2TX5MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX6, MADERA_AIF2TX6MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX7, MADERA_AIF2TX7MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF2TX8, MADERA_AIF2TX8MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(AIF3TX1, MADERA_AIF3TX1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF3TX2, MADERA_AIF3TX2MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(AIF4TX1, MADERA_AIF4TX1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(AIF4TX2, MADERA_AIF4TX2MIX_INPUT_1_SOURCE);

MADERA_MIXER_ENUMS(SLIMTX1, MADERA_SLIMTX1MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX2, MADERA_SLIMTX2MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX3, MADERA_SLIMTX3MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX4, MADERA_SLIMTX4MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX5, MADERA_SLIMTX5MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX6, MADERA_SLIMTX6MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX7, MADERA_SLIMTX7MIX_INPUT_1_SOURCE);
MADERA_MIXER_ENUMS(SLIMTX8, MADERA_SLIMTX8MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(SPD1TX1, MADERA_SPDIF1TX1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(SPD1TX2, MADERA_SPDIF1TX2MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ASRC1IN1L, MADERA_ASRC1_1LMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC1IN1R, MADERA_ASRC1_1RMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC1IN2L, MADERA_ASRC1_2LMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC1IN2R, MADERA_ASRC1_2RMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC2IN1L, MADERA_ASRC2_1LMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC2IN1R, MADERA_ASRC2_1RMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC2IN2L, MADERA_ASRC2_2LMIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ASRC2IN2R, MADERA_ASRC2_2RMIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC1INT1, MADERA_ISRC1INT1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1INT2, MADERA_ISRC1INT2MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1INT3, MADERA_ISRC1INT3MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1INT4, MADERA_ISRC1INT4MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC1DEC1, MADERA_ISRC1DEC1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1DEC2, MADERA_ISRC1DEC2MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1DEC3, MADERA_ISRC1DEC3MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC1DEC4, MADERA_ISRC1DEC4MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC2INT1, MADERA_ISRC2INT1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2INT2, MADERA_ISRC2INT2MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2INT3, MADERA_ISRC2INT3MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2INT4, MADERA_ISRC2INT4MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC2DEC1, MADERA_ISRC2DEC1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2DEC2, MADERA_ISRC2DEC2MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2DEC3, MADERA_ISRC2DEC3MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC2DEC4, MADERA_ISRC2DEC4MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC3INT1, MADERA_ISRC3INT1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC3INT2, MADERA_ISRC3INT2MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC3DEC1, MADERA_ISRC3DEC1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC3DEC2, MADERA_ISRC3DEC2MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC4INT1, MADERA_ISRC4INT1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC4INT2, MADERA_ISRC4INT2MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(ISRC4DEC1, MADERA_ISRC4DEC1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(ISRC4DEC2, MADERA_ISRC4DEC2MIX_INPUT_1_SOURCE);

MADERA_MUX_ENUMS(DFC1, MADERA_DFC1MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC2, MADERA_DFC2MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC3, MADERA_DFC3MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC4, MADERA_DFC4MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC5, MADERA_DFC5MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC6, MADERA_DFC6MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC7, MADERA_DFC7MIX_INPUT_1_SOURCE);
MADERA_MUX_ENUMS(DFC8, MADERA_DFC8MIX_INPUT_1_SOURCE);

static const char * const cs47l90_memory_mux_texts[] = {
	"None",
	"Shared Memory",
};

static const struct soc_enum cs47l90_memory_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(cs47l90_memory_mux_texts),
			cs47l90_memory_mux_texts);

static const struct snd_kcontrol_new cs47l90_memory_mux[] = {
	SOC_DAPM_ENUM("DSP2 Virtual Input", cs47l90_memory_enum),
	SOC_DAPM_ENUM("DSP3 Virtual Input", cs47l90_memory_enum),
};

static const char * const cs47l90_aec_loopback_texts[] = {
	"HPOUT1L", "HPOUT1R", "HPOUT2L", "HPOUT2R", "HPOUT3L", "HPOUT3R",
	"SPKDAT1L", "SPKDAT1R",
};

static const unsigned int cs47l90_aec_loopback_values[] = {
	0, 1, 2, 3, 4, 5, 8, 9,
};

static const struct soc_enum cs47l90_aec_loopback =
	SOC_VALUE_ENUM_SINGLE(MADERA_DAC_AEC_CONTROL_1,
			      MADERA_AEC1_LOOPBACK_SRC_SHIFT, 0xf,
			      ARRAY_SIZE(cs47l90_aec_loopback_texts),
			      cs47l90_aec_loopback_texts,
			      cs47l90_aec_loopback_values);

static const struct snd_kcontrol_new cs47l90_aec_loopback_mux =
	SOC_DAPM_ENUM("AEC1 Loopback", cs47l90_aec_loopback);

static const struct snd_kcontrol_new cs47l90_anc_input_mux[] = {
	SOC_DAPM_ENUM("RXANCL Input", madera_anc_input_src[0]),
	SOC_DAPM_ENUM("RXANCL Channel", madera_anc_input_src[1]),
	SOC_DAPM_ENUM("RXANCR Input", madera_anc_input_src[2]),
	SOC_DAPM_ENUM("RXANCR Channel", madera_anc_input_src[3]),
};

static const struct snd_kcontrol_new cs47l90_anc_ng_mux =
	SOC_DAPM_ENUM("RXANC NG Source", madera_anc_ng_enum);

static const struct snd_kcontrol_new cs47l90_output_anc_src[] = {
	SOC_DAPM_ENUM("HPOUT1L ANC Source", madera_output_anc_src[0]),
	SOC_DAPM_ENUM("HPOUT1R ANC Source", madera_output_anc_src[1]),
	SOC_DAPM_ENUM("HPOUT2L ANC Source", madera_output_anc_src[2]),
	SOC_DAPM_ENUM("HPOUT2R ANC Source", madera_output_anc_src[3]),
	SOC_DAPM_ENUM("HPOUT3L ANC Source", madera_output_anc_src[4]),
	SOC_DAPM_ENUM("HPOUT3R ANC Source", madera_output_anc_src[0]),
	SOC_DAPM_ENUM("SPKDAT1L ANC Source", madera_output_anc_src[8]),
	SOC_DAPM_ENUM("SPKDAT1R ANC Source", madera_output_anc_src[9]),
};

static const struct snd_soc_dapm_widget cs47l90_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", MADERA_SYSTEM_CLOCK_1, MADERA_SYSCLK_ENA_SHIFT,
		    0, madera_sysclk_ev,
		    SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_SUPPLY("ASYNCCLK", MADERA_ASYNC_CLOCK_1,
		    MADERA_ASYNC_CLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("OPCLK", MADERA_OUTPUT_SYSTEM_CLOCK,
		    MADERA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCOPCLK", MADERA_OUTPUT_ASYNC_CLOCK,
		    MADERA_OPCLK_ASYNC_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("DSPCLK", MADERA_DSP_CLOCK_1,
		    MADERA_DSP_CLK_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD2", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD3", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD4", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD1", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD2", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("MICVDD", 0, SND_SOC_DAPM_REGULATOR_BYPASS),

SND_SOC_DAPM_SUPPLY("MICBIAS1", MADERA_MIC_BIAS_CTRL_1,
		    MADERA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2", MADERA_MIC_BIAS_CTRL_2,
		    MADERA_MICB1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS1A", MADERA_MIC_BIAS_CTRL_5,
			MADERA_MICB1A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1B", MADERA_MIC_BIAS_CTRL_5,
			MADERA_MICB1B_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1C", MADERA_MIC_BIAS_CTRL_5,
			MADERA_MICB1C_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1D", MADERA_MIC_BIAS_CTRL_5,
			MADERA_MICB1D_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS2A", MADERA_MIC_BIAS_CTRL_6,
			MADERA_MICB2A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2B", MADERA_MIC_BIAS_CTRL_6,
			MADERA_MICB2B_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2C", MADERA_MIC_BIAS_CTRL_6,
			MADERA_MICB2C_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2D", MADERA_MIC_BIAS_CTRL_6,
			MADERA_MICB2D_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("GPSW", MADERA_GP_SWITCH_1,
		0, 0, NULL, 0),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),

SND_SOC_DAPM_INPUT("IN1AL"),
SND_SOC_DAPM_INPUT("IN1BL"),
SND_SOC_DAPM_INPUT("IN1AR"),
SND_SOC_DAPM_INPUT("IN1BR"),
SND_SOC_DAPM_INPUT("IN2AL"),
SND_SOC_DAPM_INPUT("IN2BL"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),
SND_SOC_DAPM_INPUT("IN4L"),
SND_SOC_DAPM_INPUT("IN4R"),
SND_SOC_DAPM_INPUT("IN5L"),
SND_SOC_DAPM_INPUT("IN5R"),
SND_SOC_DAPM_INPUT("DSP Virtual Input"),

SND_SOC_DAPM_OUTPUT("DRC1 Signal Activity"),
SND_SOC_DAPM_OUTPUT("DRC2 Signal Activity"),

SND_SOC_DAPM_OUTPUT("DSP1 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP2 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP3 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP4 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP5 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP6 Trigger Out"),
SND_SOC_DAPM_OUTPUT("DSP7 Trigger Out"),

SND_SOC_DAPM_OUTPUT("DSP1 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP2 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP3 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP4 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP5 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP6 Virtual Output"),
SND_SOC_DAPM_OUTPUT("DSP7 Virtual Output"),

SND_SOC_DAPM_MUX("IN1L Mux", SND_SOC_NOPM, 0, 0, &madera_inmux[0]),
SND_SOC_DAPM_MUX("IN1R Mux", SND_SOC_NOPM, 0, 0, &madera_inmux[1]),
SND_SOC_DAPM_MUX("IN2L Mux", SND_SOC_NOPM, 0, 0, &madera_inmux[2]),

SND_SOC_DAPM_PGA("PWM1 Driver", MADERA_PWM_DRIVE_1, MADERA_PWM1_ENA_SHIFT,
		 0, NULL, 0),
SND_SOC_DAPM_PGA("PWM2 Driver", MADERA_PWM_DRIVE_1, MADERA_PWM2_ENA_SHIFT,
		 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("RXANC NG External Clock", SND_SOC_NOPM,
		    MADERA_EXT_NG_SEL_SET_SHIFT, 0, madera_anc_ev,
		    SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA("RXANCL NG External", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_PGA("RXANCR NG External", SND_SOC_NOPM, 0, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("RXANC NG Clock", SND_SOC_NOPM,
		    MADERA_CLK_NG_ENA_SET_SHIFT, 0, madera_anc_ev,
		    SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA("RXANCL NG Internal", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_PGA("RXANCR NG Internal", SND_SOC_NOPM, 0, 0, NULL, 0),

SND_SOC_DAPM_MUX("RXANCL Left Input", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[0]),
SND_SOC_DAPM_MUX("RXANCL Right Input", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[0]),
SND_SOC_DAPM_MUX("RXANCL Channel", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[1]),
SND_SOC_DAPM_MUX("RXANCL NG Mux", SND_SOC_NOPM, 0, 0, &cs47l90_anc_ng_mux),
SND_SOC_DAPM_MUX("RXANCR Left Input", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[2]),
SND_SOC_DAPM_MUX("RXANCR Right Input", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[2]),
SND_SOC_DAPM_MUX("RXANCR Channel", SND_SOC_NOPM, 0, 0,
		 &cs47l90_anc_input_mux[3]),
SND_SOC_DAPM_MUX("RXANCR NG Mux", SND_SOC_NOPM, 0, 0, &cs47l90_anc_ng_mux),

SND_SOC_DAPM_PGA_E("RXANCL", SND_SOC_NOPM, MADERA_CLK_L_ENA_SET_SHIFT,
		   0, NULL, 0, madera_anc_ev,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA_E("RXANCR", SND_SOC_NOPM, MADERA_CLK_R_ENA_SET_SHIFT,
		   0, NULL, 0, madera_anc_ev,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_MUX("HPOUT1L ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[0]),
SND_SOC_DAPM_MUX("HPOUT1R ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[1]),
SND_SOC_DAPM_MUX("HPOUT2L ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[2]),
SND_SOC_DAPM_MUX("HPOUT2R ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[3]),
SND_SOC_DAPM_MUX("HPOUT3L ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[4]),
SND_SOC_DAPM_MUX("HPOUT3R ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[5]),
SND_SOC_DAPM_MUX("SPKDAT1L ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[6]),
SND_SOC_DAPM_MUX("SPKDAT1R ANC Source", SND_SOC_NOPM, 0, 0,
		 &cs47l90_output_anc_src[7]),

SND_SOC_DAPM_AIF_OUT("AIF1TX1", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX2", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX3", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX4", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX5", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX6", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX7", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX8", NULL, 0,
		     MADERA_AIF1_TX_ENABLES, MADERA_AIF1TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX3", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX4", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX5", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX6", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX7", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX8", NULL, 0,
		     MADERA_AIF2_TX_ENABLES, MADERA_AIF2TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT_E("SLIMTX1", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX1_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX2", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX2_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX3", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX3_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX4", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX4_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX5", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX5_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX6", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX6_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX7", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX7_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_OUT_E("SLIMTX8", NULL, 0,
			MADERA_SLIMBUS_TX_CHANNEL_ENABLE,
			MADERA_SLIMTX8_ENA_SHIFT, 0,
			madera_slim_tx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     MADERA_AIF3_TX_ENABLES, MADERA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     MADERA_AIF3_TX_ENABLES, MADERA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF4TX1", NULL, 0,
		     MADERA_AIF4_TX_ENABLES, MADERA_AIF4TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF4TX2", NULL, 0,
		     MADERA_AIF4_TX_ENABLES, MADERA_AIF4TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_PGA_E("OUT1L", SND_SOC_NOPM,
		   MADERA_OUT1L_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", SND_SOC_NOPM,
		   MADERA_OUT1R_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2L", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT2L_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2R", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT2R_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3L", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT3L_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3R", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT3R_ENA_SHIFT, 0, NULL, 0, madera_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5L", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT5L_ENA_SHIFT, 0, NULL, 0, madera_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5R", MADERA_OUTPUT_ENABLES_1,
		   MADERA_OUT5R_ENA_SHIFT, 0, NULL, 0, madera_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA("SPD1TX1", MADERA_SPD1_TX_CONTROL,
		   MADERA_SPD1_VAL1_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("SPD1TX2", MADERA_SPD1_TX_CONTROL,
		   MADERA_SPD1_VAL2_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_OUT_DRV("SPD1", MADERA_SPD1_TX_CONTROL,
		     MADERA_SPD1_ENA_SHIFT, 0, NULL, 0),

/* mux_in widgets : arranged in the order of sources
   specified in MADERA_MIXER_INPUT_ROUTES */

SND_SOC_DAPM_PGA("Noise Generator", MADERA_COMFORT_NOISE_GENERATOR,
		 MADERA_NOISE_GEN_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Tone Generator 1", MADERA_TONE_GENERATOR_1,
		 MADERA_TONE1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("Tone Generator 2", MADERA_TONE_GENERATOR_1,
		 MADERA_TONE2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_MIC("HAPTICS", NULL),

SND_SOC_DAPM_MUX("AEC1 Loopback", MADERA_DAC_AEC_CONTROL_1,
		       MADERA_AEC1_LOOPBACK_ENA_SHIFT, 0,
		       &cs47l90_aec_loopback_mux),

SND_SOC_DAPM_PGA_E("IN1L PGA", MADERA_INPUT_ENABLES, MADERA_IN1L_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN1R PGA", MADERA_INPUT_ENABLES, MADERA_IN1R_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2L PGA", MADERA_INPUT_ENABLES, MADERA_IN2L_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2R PGA", MADERA_INPUT_ENABLES, MADERA_IN2R_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3L PGA", MADERA_INPUT_ENABLES, MADERA_IN3L_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3R PGA", MADERA_INPUT_ENABLES, MADERA_IN3R_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN4L PGA", MADERA_INPUT_ENABLES, MADERA_IN4L_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN4R PGA", MADERA_INPUT_ENABLES, MADERA_IN4R_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN5L PGA", MADERA_INPUT_ENABLES, MADERA_IN5L_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN5R PGA", MADERA_INPUT_ENABLES, MADERA_IN5R_ENA_SHIFT,
		   0, NULL, 0, madera_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_AIF_IN("AIF1RX1", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX2", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX3", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX4", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX5", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX6", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX7", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX8", NULL, 0,
			MADERA_AIF1_RX_ENABLES, MADERA_AIF1RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX3", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX4", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX5", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX6", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX7", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX8", NULL, 0,
			MADERA_AIF2_RX_ENABLES, MADERA_AIF2RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
			MADERA_AIF3_RX_ENABLES, MADERA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
			MADERA_AIF3_RX_ENABLES, MADERA_AIF3RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF4RX1", NULL, 0,
			MADERA_AIF4_RX_ENABLES, MADERA_AIF4RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF4RX2", NULL, 0,
			MADERA_AIF4_RX_ENABLES, MADERA_AIF4RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN_E("SLIMRX1", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX1_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX2", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX2_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX3", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX3_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX4", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX4_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX5", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX5_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX6", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX6_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX7", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX7_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_AIF_IN_E("SLIMRX8", NULL, 0,
			MADERA_SLIMBUS_RX_CHANNEL_ENABLE,
			MADERA_SLIMRX8_ENA_SHIFT, 0,
			madera_slim_rx_ev,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_PGA("EQ1", MADERA_EQ1_1, MADERA_EQ1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ2", MADERA_EQ2_1, MADERA_EQ2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ3", MADERA_EQ3_1, MADERA_EQ3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ4", MADERA_EQ4_1, MADERA_EQ4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DRC1L", MADERA_DRC1_CTRL1, MADERA_DRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC1R", MADERA_DRC1_CTRL1, MADERA_DRC1R_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC2L", MADERA_DRC2_CTRL1, MADERA_DRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC2R", MADERA_DRC2_CTRL1, MADERA_DRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("LHPF1", MADERA_HPLPF1_1, MADERA_LHPF1_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF2", MADERA_HPLPF2_1, MADERA_LHPF2_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF3", MADERA_HPLPF3_1, MADERA_LHPF3_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF4", MADERA_HPLPF4_1, MADERA_LHPF4_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("ASRC1IN1L", MADERA_ASRC1_ENABLE,
		MADERA_ASRC1_IN1L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN1R", MADERA_ASRC1_ENABLE,
		MADERA_ASRC1_IN1R_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN2L", MADERA_ASRC1_ENABLE,
		MADERA_ASRC1_IN2L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN2R", MADERA_ASRC1_ENABLE,
		MADERA_ASRC1_IN2R_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC2IN1L", MADERA_ASRC2_ENABLE,
		MADERA_ASRC2_IN1L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN1R", MADERA_ASRC2_ENABLE,
		MADERA_ASRC2_IN1R_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN2L", MADERA_ASRC2_ENABLE,
		MADERA_ASRC2_IN2L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN2R", MADERA_ASRC2_ENABLE,
		MADERA_ASRC2_IN2R_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1DEC1", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC2", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC3", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_DEC3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC4", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_DEC4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1INT1", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT2", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT3", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_INT3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT4", MADERA_ISRC_1_CTRL_3,
		 MADERA_ISRC1_INT4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2DEC1", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC2", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC3", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_DEC3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC4", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_DEC4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2INT1", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT2", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT3", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_INT3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT4", MADERA_ISRC_2_CTRL_3,
		 MADERA_ISRC2_INT4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC3DEC1", MADERA_ISRC_3_CTRL_3,
		 MADERA_ISRC3_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC3DEC2", MADERA_ISRC_3_CTRL_3,
		 MADERA_ISRC3_DEC2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC3INT1", MADERA_ISRC_3_CTRL_3,
		 MADERA_ISRC3_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC3INT2", MADERA_ISRC_3_CTRL_3,
		 MADERA_ISRC3_INT2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC4DEC1", MADERA_ISRC_4_CTRL_3,
		 MADERA_ISRC4_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC4DEC2", MADERA_ISRC_4_CTRL_3,
		 MADERA_ISRC4_DEC2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC4INT1", MADERA_ISRC_4_CTRL_3,
		 MADERA_ISRC4_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC4INT2", MADERA_ISRC_4_CTRL_3,
		 MADERA_ISRC4_INT2_ENA_SHIFT, 0, NULL, 0),

WM_ADSP2("DSP1", 0, cs47l90_adsp_power_ev),
WM_ADSP2("DSP2", 1, cs47l90_adsp_power_ev),
WM_ADSP2("DSP3", 2, cs47l90_adsp_power_ev),
WM_ADSP2("DSP4", 3, cs47l90_adsp_power_ev),
WM_ADSP2("DSP5", 4, cs47l90_adsp_power_ev),
WM_ADSP2("DSP6", 5, cs47l90_adsp_power_ev),
WM_ADSP2("DSP7", 6, cs47l90_adsp_power_ev),

/* end of ordered widget list */

SND_SOC_DAPM_PGA("DFC1", MADERA_DFC1_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC2", MADERA_DFC2_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC3", MADERA_DFC3_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC4", MADERA_DFC4_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC5", MADERA_DFC5_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC6", MADERA_DFC6_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC7", MADERA_DFC7_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC8", MADERA_DFC8_CTRL, MADERA_DFC1_ENA_SHIFT, 0, NULL, 0),

MADERA_MIXER_WIDGETS(EQ1, "EQ1"),
MADERA_MIXER_WIDGETS(EQ2, "EQ2"),
MADERA_MIXER_WIDGETS(EQ3, "EQ3"),
MADERA_MIXER_WIDGETS(EQ4, "EQ4"),

MADERA_MIXER_WIDGETS(DRC1L, "DRC1L"),
MADERA_MIXER_WIDGETS(DRC1R, "DRC1R"),
MADERA_MIXER_WIDGETS(DRC2L, "DRC2L"),
MADERA_MIXER_WIDGETS(DRC2R, "DRC2R"),

SND_SOC_DAPM_SWITCH("DRC1 Activity Output", SND_SOC_NOPM, 0, 0,
		    &madera_drc_activity_output_mux[0]),
SND_SOC_DAPM_SWITCH("DRC2 Activity Output", SND_SOC_NOPM, 0, 0,
		    &madera_drc_activity_output_mux[1]),

MADERA_MIXER_WIDGETS(LHPF1, "LHPF1"),
MADERA_MIXER_WIDGETS(LHPF2, "LHPF2"),
MADERA_MIXER_WIDGETS(LHPF3, "LHPF3"),
MADERA_MIXER_WIDGETS(LHPF4, "LHPF4"),

MADERA_MIXER_WIDGETS(PWM1, "PWM1"),
MADERA_MIXER_WIDGETS(PWM2, "PWM2"),

MADERA_MIXER_WIDGETS(OUT1L, "HPOUT1L"),
MADERA_MIXER_WIDGETS(OUT1R, "HPOUT1R"),
MADERA_MIXER_WIDGETS(OUT2L, "HPOUT2L"),
MADERA_MIXER_WIDGETS(OUT2R, "HPOUT2R"),
MADERA_MIXER_WIDGETS(OUT3L, "HPOUT3L"),
MADERA_MIXER_WIDGETS(OUT3R, "HPOUT3R"),
MADERA_MIXER_WIDGETS(SPKDAT1L, "SPKDAT1L"),
MADERA_MIXER_WIDGETS(SPKDAT1R, "SPKDAT1R"),

MADERA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
MADERA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),
MADERA_MIXER_WIDGETS(AIF1TX3, "AIF1TX3"),
MADERA_MIXER_WIDGETS(AIF1TX4, "AIF1TX4"),
MADERA_MIXER_WIDGETS(AIF1TX5, "AIF1TX5"),
MADERA_MIXER_WIDGETS(AIF1TX6, "AIF1TX6"),
MADERA_MIXER_WIDGETS(AIF1TX7, "AIF1TX7"),
MADERA_MIXER_WIDGETS(AIF1TX8, "AIF1TX8"),

MADERA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
MADERA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),
MADERA_MIXER_WIDGETS(AIF2TX3, "AIF2TX3"),
MADERA_MIXER_WIDGETS(AIF2TX4, "AIF2TX4"),
MADERA_MIXER_WIDGETS(AIF2TX5, "AIF2TX5"),
MADERA_MIXER_WIDGETS(AIF2TX6, "AIF2TX6"),
MADERA_MIXER_WIDGETS(AIF2TX7, "AIF2TX7"),
MADERA_MIXER_WIDGETS(AIF2TX8, "AIF2TX8"),

MADERA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
MADERA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

MADERA_MIXER_WIDGETS(AIF4TX1, "AIF4TX1"),
MADERA_MIXER_WIDGETS(AIF4TX2, "AIF4TX2"),

MADERA_MIXER_WIDGETS(SLIMTX1, "SLIMTX1"),
MADERA_MIXER_WIDGETS(SLIMTX2, "SLIMTX2"),
MADERA_MIXER_WIDGETS(SLIMTX3, "SLIMTX3"),
MADERA_MIXER_WIDGETS(SLIMTX4, "SLIMTX4"),
MADERA_MIXER_WIDGETS(SLIMTX5, "SLIMTX5"),
MADERA_MIXER_WIDGETS(SLIMTX6, "SLIMTX6"),
MADERA_MIXER_WIDGETS(SLIMTX7, "SLIMTX7"),
MADERA_MIXER_WIDGETS(SLIMTX8, "SLIMTX8"),

MADERA_MUX_WIDGETS(SPD1TX1, "SPDIF1TX1"),
MADERA_MUX_WIDGETS(SPD1TX2, "SPDIF1TX2"),

MADERA_MUX_WIDGETS(ASRC1IN1L, "ASRC1IN1L"),
MADERA_MUX_WIDGETS(ASRC1IN1R, "ASRC1IN1R"),
MADERA_MUX_WIDGETS(ASRC1IN2L, "ASRC1IN2L"),
MADERA_MUX_WIDGETS(ASRC1IN2R, "ASRC1IN2R"),
MADERA_MUX_WIDGETS(ASRC2IN1L, "ASRC2IN1L"),
MADERA_MUX_WIDGETS(ASRC2IN1R, "ASRC2IN1R"),
MADERA_MUX_WIDGETS(ASRC2IN2L, "ASRC2IN2L"),
MADERA_MUX_WIDGETS(ASRC2IN2R, "ASRC2IN2R"),

MADERA_DSP_WIDGETS(DSP1, "DSP1"),
MADERA_DSP_WIDGETS(DSP2, "DSP2"),
MADERA_DSP_WIDGETS(DSP3, "DSP3"),
MADERA_DSP_WIDGETS(DSP4, "DSP4"),
MADERA_DSP_WIDGETS(DSP5, "DSP5"),
MADERA_DSP_WIDGETS(DSP6, "DSP6"),
MADERA_DSP_WIDGETS(DSP7, "DSP7"),

SND_SOC_DAPM_MUX("DSP2 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &cs47l90_memory_mux[0]),
SND_SOC_DAPM_MUX("DSP3 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &cs47l90_memory_mux[1]),

SND_SOC_DAPM_SWITCH("DSP1 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[0]),
SND_SOC_DAPM_SWITCH("DSP2 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[1]),
SND_SOC_DAPM_SWITCH("DSP3 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[2]),
SND_SOC_DAPM_SWITCH("DSP4 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[3]),
SND_SOC_DAPM_SWITCH("DSP5 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[4]),
SND_SOC_DAPM_SWITCH("DSP6 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[5]),
SND_SOC_DAPM_SWITCH("DSP7 Trigger Output", SND_SOC_NOPM, 0, 0,
		    &madera_dsp_trigger_output_mux[6]),

MADERA_MUX_WIDGETS(ISRC1DEC1, "ISRC1DEC1"),
MADERA_MUX_WIDGETS(ISRC1DEC2, "ISRC1DEC2"),
MADERA_MUX_WIDGETS(ISRC1DEC3, "ISRC1DEC3"),
MADERA_MUX_WIDGETS(ISRC1DEC4, "ISRC1DEC4"),

MADERA_MUX_WIDGETS(ISRC1INT1, "ISRC1INT1"),
MADERA_MUX_WIDGETS(ISRC1INT2, "ISRC1INT2"),
MADERA_MUX_WIDGETS(ISRC1INT3, "ISRC1INT3"),
MADERA_MUX_WIDGETS(ISRC1INT4, "ISRC1INT4"),

MADERA_MUX_WIDGETS(ISRC2DEC1, "ISRC2DEC1"),
MADERA_MUX_WIDGETS(ISRC2DEC2, "ISRC2DEC2"),
MADERA_MUX_WIDGETS(ISRC2DEC3, "ISRC2DEC3"),
MADERA_MUX_WIDGETS(ISRC2DEC4, "ISRC2DEC4"),

MADERA_MUX_WIDGETS(ISRC2INT1, "ISRC2INT1"),
MADERA_MUX_WIDGETS(ISRC2INT2, "ISRC2INT2"),
MADERA_MUX_WIDGETS(ISRC2INT3, "ISRC2INT3"),
MADERA_MUX_WIDGETS(ISRC2INT4, "ISRC2INT4"),

MADERA_MUX_WIDGETS(ISRC3DEC1, "ISRC3DEC1"),
MADERA_MUX_WIDGETS(ISRC3DEC2, "ISRC3DEC2"),

MADERA_MUX_WIDGETS(ISRC3INT1, "ISRC3INT1"),
MADERA_MUX_WIDGETS(ISRC3INT2, "ISRC3INT2"),

MADERA_MUX_WIDGETS(ISRC4DEC1, "ISRC4DEC1"),
MADERA_MUX_WIDGETS(ISRC4DEC2, "ISRC4DEC2"),

MADERA_MUX_WIDGETS(ISRC4INT1, "ISRC4INT1"),
MADERA_MUX_WIDGETS(ISRC4INT2, "ISRC4INT2"),

MADERA_MUX_WIDGETS(DFC1, "DFC1"),
MADERA_MUX_WIDGETS(DFC2, "DFC2"),
MADERA_MUX_WIDGETS(DFC3, "DFC3"),
MADERA_MUX_WIDGETS(DFC4, "DFC4"),
MADERA_MUX_WIDGETS(DFC5, "DFC5"),
MADERA_MUX_WIDGETS(DFC6, "DFC6"),
MADERA_MUX_WIDGETS(DFC7, "DFC7"),
MADERA_MUX_WIDGETS(DFC8, "DFC8"),

SND_SOC_DAPM_OUTPUT("HPOUT1L"),
SND_SOC_DAPM_OUTPUT("HPOUT1R"),
SND_SOC_DAPM_OUTPUT("HPOUT2L"),
SND_SOC_DAPM_OUTPUT("HPOUT2R"),
SND_SOC_DAPM_OUTPUT("HPOUT3L"),
SND_SOC_DAPM_OUTPUT("HPOUT3R"),
SND_SOC_DAPM_OUTPUT("SPKDAT1L"),
SND_SOC_DAPM_OUTPUT("SPKDAT1R"),
SND_SOC_DAPM_OUTPUT("SPDIF1"),

SND_SOC_DAPM_OUTPUT("MICSUPP"),
};

#define MADERA_MIXER_INPUT_ROUTES(name)	\
	{ name, "Noise Generator", "Noise Generator" }, \
	{ name, "Tone Generator 1", "Tone Generator 1" }, \
	{ name, "Tone Generator 2", "Tone Generator 2" }, \
	{ name, "Haptics", "HAPTICS" }, \
	{ name, "AEC1", "AEC1 Loopback" }, \
	{ name, "IN1L", "IN1L PGA" }, \
	{ name, "IN1R", "IN1R PGA" }, \
	{ name, "IN2L", "IN2L PGA" }, \
	{ name, "IN2R", "IN2R PGA" }, \
	{ name, "IN3L", "IN3L PGA" }, \
	{ name, "IN3R", "IN3R PGA" }, \
	{ name, "IN4L", "IN4L PGA" }, \
	{ name, "IN4R", "IN4R PGA" }, \
	{ name, "IN5L", "IN5L PGA" }, \
	{ name, "IN5R", "IN5R PGA" }, \
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF1RX3", "AIF1RX3" }, \
	{ name, "AIF1RX4", "AIF1RX4" }, \
	{ name, "AIF1RX5", "AIF1RX5" }, \
	{ name, "AIF1RX6", "AIF1RX6" }, \
	{ name, "AIF1RX7", "AIF1RX7" }, \
	{ name, "AIF1RX8", "AIF1RX8" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF2RX3", "AIF2RX3" }, \
	{ name, "AIF2RX4", "AIF2RX4" }, \
	{ name, "AIF2RX5", "AIF2RX5" }, \
	{ name, "AIF2RX6", "AIF2RX6" }, \
	{ name, "AIF2RX7", "AIF2RX7" }, \
	{ name, "AIF2RX8", "AIF2RX8" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "AIF4RX1", "AIF4RX1" }, \
	{ name, "AIF4RX2", "AIF4RX2" }, \
	{ name, "SLIMRX1", "SLIMRX1" }, \
	{ name, "SLIMRX2", "SLIMRX2" }, \
	{ name, "SLIMRX3", "SLIMRX3" }, \
	{ name, "SLIMRX4", "SLIMRX4" }, \
	{ name, "SLIMRX5", "SLIMRX5" }, \
	{ name, "SLIMRX6", "SLIMRX6" }, \
	{ name, "SLIMRX7", "SLIMRX7" }, \
	{ name, "SLIMRX8", "SLIMRX8" }, \
	{ name, "EQ1", "EQ1" }, \
	{ name, "EQ2", "EQ2" }, \
	{ name, "EQ3", "EQ3" }, \
	{ name, "EQ4", "EQ4" }, \
	{ name, "DRC1L", "DRC1L" }, \
	{ name, "DRC1R", "DRC1R" }, \
	{ name, "DRC2L", "DRC2L" }, \
	{ name, "DRC2R", "DRC2R" }, \
	{ name, "LHPF1", "LHPF1" }, \
	{ name, "LHPF2", "LHPF2" }, \
	{ name, "LHPF3", "LHPF3" }, \
	{ name, "LHPF4", "LHPF4" }, \
	{ name, "ASRC1IN1L", "ASRC1IN1L" }, \
	{ name, "ASRC1IN1R", "ASRC1IN1R" }, \
	{ name, "ASRC1IN2L", "ASRC1IN2L" }, \
	{ name, "ASRC1IN2R", "ASRC1IN2R" }, \
	{ name, "ASRC2IN1L", "ASRC2IN1L" }, \
	{ name, "ASRC2IN1R", "ASRC2IN1R" }, \
	{ name, "ASRC2IN2L", "ASRC2IN2L" }, \
	{ name, "ASRC2IN2R", "ASRC2IN2R" }, \
	{ name, "ISRC1DEC1", "ISRC1DEC1" }, \
	{ name, "ISRC1DEC2", "ISRC1DEC2" }, \
	{ name, "ISRC1DEC3", "ISRC1DEC3" }, \
	{ name, "ISRC1DEC4", "ISRC1DEC4" }, \
	{ name, "ISRC1INT1", "ISRC1INT1" }, \
	{ name, "ISRC1INT2", "ISRC1INT2" }, \
	{ name, "ISRC1INT3", "ISRC1INT3" }, \
	{ name, "ISRC1INT4", "ISRC1INT4" }, \
	{ name, "ISRC2DEC1", "ISRC2DEC1" }, \
	{ name, "ISRC2DEC2", "ISRC2DEC2" }, \
	{ name, "ISRC2DEC3", "ISRC2DEC3" }, \
	{ name, "ISRC2DEC4", "ISRC2DEC4" }, \
	{ name, "ISRC2INT1", "ISRC2INT1" }, \
	{ name, "ISRC2INT2", "ISRC2INT2" }, \
	{ name, "ISRC2INT3", "ISRC2INT3" }, \
	{ name, "ISRC2INT4", "ISRC2INT4" }, \
	{ name, "ISRC3DEC1", "ISRC3DEC1" }, \
	{ name, "ISRC3DEC2", "ISRC3DEC2" }, \
	{ name, "ISRC3INT1", "ISRC3INT1" }, \
	{ name, "ISRC3INT2", "ISRC3INT2" }, \
	{ name, "ISRC4DEC1", "ISRC4DEC1" }, \
	{ name, "ISRC4DEC2", "ISRC4DEC2" }, \
	{ name, "ISRC4INT1", "ISRC4INT1" }, \
	{ name, "ISRC4INT2", "ISRC4INT2" }, \
	{ name, "DSP1.1", "DSP1" }, \
	{ name, "DSP1.2", "DSP1" }, \
	{ name, "DSP1.3", "DSP1" }, \
	{ name, "DSP1.4", "DSP1" }, \
	{ name, "DSP1.5", "DSP1" }, \
	{ name, "DSP1.6", "DSP1" }, \
	{ name, "DSP2.1", "DSP2" }, \
	{ name, "DSP2.2", "DSP2" }, \
	{ name, "DSP2.3", "DSP2" }, \
	{ name, "DSP2.4", "DSP2" }, \
	{ name, "DSP2.5", "DSP2" }, \
	{ name, "DSP2.6", "DSP2" }, \
	{ name, "DSP3.1", "DSP3" }, \
	{ name, "DSP3.2", "DSP3" }, \
	{ name, "DSP3.3", "DSP3" }, \
	{ name, "DSP3.4", "DSP3" }, \
	{ name, "DSP3.5", "DSP3" }, \
	{ name, "DSP3.6", "DSP3" }, \
	{ name, "DSP4.1", "DSP4" }, \
	{ name, "DSP4.2", "DSP4" }, \
	{ name, "DSP4.3", "DSP4" }, \
	{ name, "DSP4.4", "DSP4" }, \
	{ name, "DSP4.5", "DSP4" }, \
	{ name, "DSP4.6", "DSP4" }, \
	{ name, "DSP5.1", "DSP5" }, \
	{ name, "DSP5.2", "DSP5" }, \
	{ name, "DSP5.3", "DSP5" }, \
	{ name, "DSP5.4", "DSP5" }, \
	{ name, "DSP5.5", "DSP5" }, \
	{ name, "DSP5.6", "DSP5" }, \
	{ name, "DSP6.1", "DSP6" }, \
	{ name, "DSP6.2", "DSP6" }, \
	{ name, "DSP6.3", "DSP6" }, \
	{ name, "DSP6.4", "DSP6" }, \
	{ name, "DSP6.5", "DSP6" }, \
	{ name, "DSP6.6", "DSP6" }, \
	{ name, "DSP7.1", "DSP7" }, \
	{ name, "DSP7.2", "DSP7" }, \
	{ name, "DSP7.3", "DSP7" }, \
	{ name, "DSP7.4", "DSP7" }, \
	{ name, "DSP7.5", "DSP7" }, \
	{ name, "DSP7.6", "DSP7" }, \
	{ name, "DFC1", "DFC1" }, \
	{ name, "DFC2", "DFC2" }, \
	{ name, "DFC3", "DFC3" }, \
	{ name, "DFC4", "DFC4" }, \
	{ name, "DFC5", "DFC5" }, \
	{ name, "DFC6", "DFC6" }, \
	{ name, "DFC7", "DFC7" }, \
	{ name, "DFC8", "DFC8" }

static const struct snd_soc_dapm_route cs47l90_dapm_routes[] = {
	{ "AIF2 Capture", NULL, "DBVDD2" },
	{ "AIF2 Playback", NULL, "DBVDD2" },

	{ "AIF3 Capture", NULL, "DBVDD3" },
	{ "AIF3 Playback", NULL, "DBVDD3" },

	{ "AIF4 Capture", NULL, "DBVDD3" },
	{ "AIF4 Playback", NULL, "DBVDD3" },

	{ "OUT1L", NULL, "CPVDD1" },
	{ "OUT1L", NULL, "CPVDD2" },
	{ "OUT1R", NULL, "CPVDD1" },
	{ "OUT1R", NULL, "CPVDD2" },
	{ "OUT2L", NULL, "CPVDD1" },
	{ "OUT2L", NULL, "CPVDD2" },
	{ "OUT2R", NULL, "CPVDD1" },
	{ "OUT2R", NULL, "CPVDD2" },
	{ "OUT3L", NULL, "CPVDD1" },
	{ "OUT3L", NULL, "CPVDD2" },
	{ "OUT3R", NULL, "CPVDD1" },
	{ "OUT3R", NULL, "CPVDD2" },

	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
	{ "OUT2L", NULL, "SYSCLK" },
	{ "OUT2R", NULL, "SYSCLK" },
	{ "OUT3L", NULL, "SYSCLK" },
	{ "OUT3R", NULL, "SYSCLK" },
	{ "OUT5L", NULL, "SYSCLK" },
	{ "OUT5R", NULL, "SYSCLK" },

	{ "SPD1", NULL, "SYSCLK" },
	{ "SPD1", NULL, "SPD1TX1" },
	{ "SPD1", NULL, "SPD1TX2" },

	{ "IN1AL", NULL, "SYSCLK" },
	{ "IN1BL", NULL, "SYSCLK" },
	{ "IN1AR", NULL, "SYSCLK" },
	{ "IN1BR", NULL, "SYSCLK" },
	{ "IN2AL", NULL, "SYSCLK" },
	{ "IN2BL", NULL, "SYSCLK" },
	{ "IN2R", NULL, "SYSCLK" },
	{ "IN3L", NULL, "SYSCLK" },
	{ "IN3R", NULL, "SYSCLK" },
	{ "IN4L", NULL, "SYSCLK" },
	{ "IN4R", NULL, "SYSCLK" },
	{ "IN5L", NULL, "SYSCLK" },
	{ "IN5R", NULL, "SYSCLK" },

	{ "IN3L", NULL, "DBVDD4" },
	{ "IN3R", NULL, "DBVDD4" },
	{ "IN4L", NULL, "DBVDD4" },
	{ "IN4R", NULL, "DBVDD4" },
	{ "IN5L", NULL, "DBVDD4" },
	{ "IN5R", NULL, "DBVDD4" },

	{ "ASRC1IN1L", NULL, "SYSCLK" },
	{ "ASRC1IN1R", NULL, "SYSCLK" },
	{ "ASRC1IN2L", NULL, "SYSCLK" },
	{ "ASRC1IN2R", NULL, "SYSCLK" },
	{ "ASRC2IN1L", NULL, "SYSCLK" },
	{ "ASRC2IN1R", NULL, "SYSCLK" },
	{ "ASRC2IN2L", NULL, "SYSCLK" },
	{ "ASRC2IN2R", NULL, "SYSCLK" },

	{ "ASRC1IN1L", NULL, "ASYNCCLK" },
	{ "ASRC1IN1R", NULL, "ASYNCCLK" },
	{ "ASRC1IN2L", NULL, "ASYNCCLK" },
	{ "ASRC1IN2R", NULL, "ASYNCCLK" },
	{ "ASRC2IN1L", NULL, "ASYNCCLK" },
	{ "ASRC2IN1R", NULL, "ASYNCCLK" },
	{ "ASRC2IN2L", NULL, "ASYNCCLK" },
	{ "ASRC2IN2R", NULL, "ASYNCCLK" },

	{ "DSP1", NULL, "DSPCLK"},
	{ "DSP2", NULL, "DSPCLK"},
	{ "DSP3", NULL, "DSPCLK"},
	{ "DSP4", NULL, "DSPCLK"},
	{ "DSP5", NULL, "DSPCLK"},
	{ "DSP6", NULL, "DSPCLK"},
	{ "DSP7", NULL, "DSPCLK"},

	{ "MICBIAS1", NULL, "MICVDD" },
	{ "MICBIAS2", NULL, "MICVDD" },

	{ "MICBIAS1A", NULL, "MICBIAS1" },
	{ "MICBIAS1B", NULL, "MICBIAS1" },
	{ "MICBIAS1C", NULL, "MICBIAS1" },
	{ "MICBIAS1D", NULL, "MICBIAS1" },

	{ "MICBIAS2A", NULL, "MICBIAS2" },
	{ "MICBIAS2B", NULL, "MICBIAS2" },
	{ "MICBIAS2C", NULL, "MICBIAS2" },
	{ "MICBIAS2D", NULL, "MICBIAS2" },

	{ "Noise Generator", NULL, "SYSCLK" },
	{ "Tone Generator 1", NULL, "SYSCLK" },
	{ "Tone Generator 2", NULL, "SYSCLK" },

	{ "Noise Generator", NULL, "NOISE" },
	{ "Tone Generator 1", NULL, "TONE" },
	{ "Tone Generator 2", NULL, "TONE" },

	{ "AIF1 Capture", NULL, "AIF1TX1" },
	{ "AIF1 Capture", NULL, "AIF1TX2" },
	{ "AIF1 Capture", NULL, "AIF1TX3" },
	{ "AIF1 Capture", NULL, "AIF1TX4" },
	{ "AIF1 Capture", NULL, "AIF1TX5" },
	{ "AIF1 Capture", NULL, "AIF1TX6" },
	{ "AIF1 Capture", NULL, "AIF1TX7" },
	{ "AIF1 Capture", NULL, "AIF1TX8" },

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },
	{ "AIF1RX3", NULL, "AIF1 Playback" },
	{ "AIF1RX4", NULL, "AIF1 Playback" },
	{ "AIF1RX5", NULL, "AIF1 Playback" },
	{ "AIF1RX6", NULL, "AIF1 Playback" },
	{ "AIF1RX7", NULL, "AIF1 Playback" },
	{ "AIF1RX8", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },
	{ "AIF2 Capture", NULL, "AIF2TX3" },
	{ "AIF2 Capture", NULL, "AIF2TX4" },
	{ "AIF2 Capture", NULL, "AIF2TX5" },
	{ "AIF2 Capture", NULL, "AIF2TX6" },
	{ "AIF2 Capture", NULL, "AIF2TX7" },
	{ "AIF2 Capture", NULL, "AIF2TX8" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },
	{ "AIF2RX3", NULL, "AIF2 Playback" },
	{ "AIF2RX4", NULL, "AIF2 Playback" },
	{ "AIF2RX5", NULL, "AIF2 Playback" },
	{ "AIF2RX6", NULL, "AIF2 Playback" },
	{ "AIF2RX7", NULL, "AIF2 Playback" },
	{ "AIF2RX8", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

	{ "AIF4 Capture", NULL, "AIF4TX1" },
	{ "AIF4 Capture", NULL, "AIF4TX2" },

	{ "AIF4RX1", NULL, "AIF4 Playback" },
	{ "AIF4RX2", NULL, "AIF4 Playback" },

	{ "Slim1 Capture", NULL, "SLIMTX1" },
	{ "Slim1 Capture", NULL, "SLIMTX2" },
	{ "Slim1 Capture", NULL, "SLIMTX3" },
	{ "Slim1 Capture", NULL, "SLIMTX4" },

	{ "SLIMRX1", NULL, "Slim1 Playback" },
	{ "SLIMRX2", NULL, "Slim1 Playback" },
	{ "SLIMRX3", NULL, "Slim1 Playback" },
	{ "SLIMRX4", NULL, "Slim1 Playback" },

	{ "Slim2 Capture", NULL, "SLIMTX5" },
	{ "Slim2 Capture", NULL, "SLIMTX6" },

	{ "SLIMRX5", NULL, "Slim2 Playback" },
	{ "SLIMRX6", NULL, "Slim2 Playback" },

	{ "Slim3 Capture", NULL, "SLIMTX7" },
	{ "Slim3 Capture", NULL, "SLIMTX8" },

	{ "SLIMRX7", NULL, "Slim3 Playback" },
	{ "SLIMRX8", NULL, "Slim3 Playback" },

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },
	{ "AIF4 Playback", NULL, "SYSCLK" },
	{ "Slim1 Playback", NULL, "SYSCLK" },
	{ "Slim2 Playback", NULL, "SYSCLK" },
	{ "Slim3 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },
	{ "AIF4 Capture", NULL, "SYSCLK" },
	{ "Slim1 Capture", NULL, "SYSCLK" },
	{ "Slim2 Capture", NULL, "SYSCLK" },
	{ "Slim3 Capture", NULL, "SYSCLK" },

	{ "Voice Control CPU", NULL, "Voice Control DSP" },
	{ "Voice Control DSP", NULL, "DSP3" },
	{ "Voice Control CPU", NULL, "SYSCLK" },
	{ "Voice Control DSP", NULL, "SYSCLK" },

	{ "Audio Trace DSP", NULL, "DSP1" },
	{ "Audio Trace DSP", NULL, "SYSCLK" },

	{ "IN1L Mux", "A", "IN1AL" },
	{ "IN1L Mux", "B", "IN1BL" },
	{ "IN1R Mux", "A", "IN1AR" },
	{ "IN1R Mux", "B", "IN1BR" },

	{ "IN2L Mux", "A", "IN2AL" },
	{ "IN2L Mux", "B", "IN2BL" },

	{ "IN1L PGA", NULL, "IN1L Mux" },
	{ "IN1R PGA", NULL, "IN1R Mux" },

	{ "IN2L PGA", NULL, "IN2L Mux" },
	{ "IN2R PGA", NULL, "IN2R" },

	{ "IN3L PGA", NULL, "IN3L" },
	{ "IN3R PGA", NULL, "IN3R" },

	{ "IN4L PGA", NULL, "IN4L" },
	{ "IN4R PGA", NULL, "IN4R" },

	{ "IN5L PGA", NULL, "IN5L" },
	{ "IN5R PGA", NULL, "IN5R" },

	MADERA_MIXER_ROUTES("OUT1L", "HPOUT1L"),
	MADERA_MIXER_ROUTES("OUT1R", "HPOUT1R"),
	MADERA_MIXER_ROUTES("OUT2L", "HPOUT2L"),
	MADERA_MIXER_ROUTES("OUT2R", "HPOUT2R"),
	MADERA_MIXER_ROUTES("OUT3L", "HPOUT3L"),
	MADERA_MIXER_ROUTES("OUT3R", "HPOUT3R"),

	MADERA_MIXER_ROUTES("OUT5L", "SPKDAT1L"),
	MADERA_MIXER_ROUTES("OUT5R", "SPKDAT1R"),

	MADERA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	MADERA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	MADERA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	MADERA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),
	MADERA_MIXER_ROUTES("AIF1TX3", "AIF1TX3"),
	MADERA_MIXER_ROUTES("AIF1TX4", "AIF1TX4"),
	MADERA_MIXER_ROUTES("AIF1TX5", "AIF1TX5"),
	MADERA_MIXER_ROUTES("AIF1TX6", "AIF1TX6"),
	MADERA_MIXER_ROUTES("AIF1TX7", "AIF1TX7"),
	MADERA_MIXER_ROUTES("AIF1TX8", "AIF1TX8"),

	MADERA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	MADERA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),
	MADERA_MIXER_ROUTES("AIF2TX3", "AIF2TX3"),
	MADERA_MIXER_ROUTES("AIF2TX4", "AIF2TX4"),
	MADERA_MIXER_ROUTES("AIF2TX5", "AIF2TX5"),
	MADERA_MIXER_ROUTES("AIF2TX6", "AIF2TX6"),
	MADERA_MIXER_ROUTES("AIF2TX7", "AIF2TX7"),
	MADERA_MIXER_ROUTES("AIF2TX8", "AIF2TX8"),

	MADERA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	MADERA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	MADERA_MIXER_ROUTES("AIF4TX1", "AIF4TX1"),
	MADERA_MIXER_ROUTES("AIF4TX2", "AIF4TX2"),

	MADERA_MIXER_ROUTES("SLIMTX1", "SLIMTX1"),
	MADERA_MIXER_ROUTES("SLIMTX2", "SLIMTX2"),
	MADERA_MIXER_ROUTES("SLIMTX3", "SLIMTX3"),
	MADERA_MIXER_ROUTES("SLIMTX4", "SLIMTX4"),
	MADERA_MIXER_ROUTES("SLIMTX5", "SLIMTX5"),
	MADERA_MIXER_ROUTES("SLIMTX6", "SLIMTX6"),
	MADERA_MIXER_ROUTES("SLIMTX7", "SLIMTX7"),
	MADERA_MIXER_ROUTES("SLIMTX8", "SLIMTX8"),

	MADERA_MUX_ROUTES("SPD1TX1", "SPDIF1TX1"),
	MADERA_MUX_ROUTES("SPD1TX2", "SPDIF1TX2"),

	MADERA_MIXER_ROUTES("EQ1", "EQ1"),
	MADERA_MIXER_ROUTES("EQ2", "EQ2"),
	MADERA_MIXER_ROUTES("EQ3", "EQ3"),
	MADERA_MIXER_ROUTES("EQ4", "EQ4"),

	MADERA_MIXER_ROUTES("DRC1L", "DRC1L"),
	MADERA_MIXER_ROUTES("DRC1R", "DRC1R"),
	MADERA_MIXER_ROUTES("DRC2L", "DRC2L"),
	MADERA_MIXER_ROUTES("DRC2R", "DRC2R"),

	MADERA_MIXER_ROUTES("LHPF1", "LHPF1"),
	MADERA_MIXER_ROUTES("LHPF2", "LHPF2"),
	MADERA_MIXER_ROUTES("LHPF3", "LHPF3"),
	MADERA_MIXER_ROUTES("LHPF4", "LHPF4"),

	MADERA_MUX_ROUTES("ASRC1IN1L", "ASRC1IN1L"),
	MADERA_MUX_ROUTES("ASRC1IN1R", "ASRC1IN1R"),
	MADERA_MUX_ROUTES("ASRC1IN2L", "ASRC1IN2L"),
	MADERA_MUX_ROUTES("ASRC1IN2R", "ASRC1IN2R"),
	MADERA_MUX_ROUTES("ASRC2IN1L", "ASRC2IN1L"),
	MADERA_MUX_ROUTES("ASRC2IN1R", "ASRC2IN1R"),
	MADERA_MUX_ROUTES("ASRC2IN2L", "ASRC2IN2L"),
	MADERA_MUX_ROUTES("ASRC2IN2R", "ASRC2IN2R"),

	MADERA_DSP_ROUTES("DSP1"),
	MADERA_DSP_ROUTES("DSP2"),
	MADERA_DSP_ROUTES("DSP3"),
	MADERA_DSP_ROUTES("DSP4"),
	MADERA_DSP_ROUTES("DSP5"),
	MADERA_DSP_ROUTES("DSP6"),
	MADERA_DSP_ROUTES("DSP7"),

	{ "DSP2 Preloader",  NULL, "DSP2 Virtual Input" },
	{ "DSP2 Virtual Input", "Shared Memory", "DSP3" },
	{ "DSP3 Preloader", NULL, "DSP3 Virtual Input" },
	{ "DSP3 Virtual Input", "Shared Memory", "DSP2" },

	{ "DSP1 Trigger Out", NULL, "SYSCLK" },
	{ "DSP1 Trigger Out", NULL, "DSP1 Trigger Output" },
	{ "DSP2 Trigger Out", NULL, "DSP2 Trigger Output" },
	{ "DSP3 Trigger Out", NULL, "DSP3 Trigger Output" },
	{ "DSP4 Trigger Out", NULL, "DSP4 Trigger Output" },
	{ "DSP5 Trigger Out", NULL, "DSP5 Trigger Output" },
	{ "DSP6 Trigger Out", NULL, "DSP6 Trigger Output" },
	{ "DSP7 Trigger Out", NULL, "DSP7 Trigger Output" },

	{ "DSP1 Trigger Output", "Switch", "DSP1" },
	{ "DSP2 Trigger Output", "Switch", "DSP2" },
	{ "DSP3 Trigger Output", "Switch", "DSP3" },
	{ "DSP4 Trigger Output", "Switch", "DSP4" },
	{ "DSP5 Trigger Output", "Switch", "DSP5" },
	{ "DSP6 Trigger Output", "Switch", "DSP6" },
	{ "DSP7 Trigger Output", "Switch", "DSP7" },

	{ "DSP1 Preloader", NULL, "DSP Virtual Input" },
	{ "DSP1 Trigger Out", NULL, "DSP1 Virtual Output" },
	{ "DSP1 Virtual Output", NULL, "SYSCLK" },

	{ "DSP2 Preloader", NULL, "DSP Virtual Input" },
	{ "DSP2 Trigger Out", NULL, "DSP2 Virtual Output" },
	{ "DSP2 Virtual Output", NULL, "SYSCLK" },

	{ "DSP3 Preloader", NULL, "DSP Virtual Input" },
	{ "DSP3 Trigger Out", NULL, "DSP3 Virtual Output" },
	{ "DSP3 Virtual Output", NULL, "SYSCLK" },

	{"LHPF1 Input 1", NULL, "DSP Virtual Input"},
	{"LHPF2 Input 1", NULL, "DSP Virtual Input"},
	{"LHPF3 Input 1", NULL, "DSP Virtual Input"},

	MADERA_MUX_ROUTES("ISRC1INT1", "ISRC1INT1"),
	MADERA_MUX_ROUTES("ISRC1INT2", "ISRC1INT2"),
	MADERA_MUX_ROUTES("ISRC1INT3", "ISRC1INT3"),
	MADERA_MUX_ROUTES("ISRC1INT4", "ISRC1INT4"),

	MADERA_MUX_ROUTES("ISRC1DEC1", "ISRC1DEC1"),
	MADERA_MUX_ROUTES("ISRC1DEC2", "ISRC1DEC2"),
	MADERA_MUX_ROUTES("ISRC1DEC3", "ISRC1DEC3"),
	MADERA_MUX_ROUTES("ISRC1DEC4", "ISRC1DEC4"),

	MADERA_MUX_ROUTES("ISRC2INT1", "ISRC2INT1"),
	MADERA_MUX_ROUTES("ISRC2INT2", "ISRC2INT2"),
	MADERA_MUX_ROUTES("ISRC2INT3", "ISRC2INT3"),
	MADERA_MUX_ROUTES("ISRC2INT4", "ISRC2INT4"),

	MADERA_MUX_ROUTES("ISRC2DEC1", "ISRC2DEC1"),
	MADERA_MUX_ROUTES("ISRC2DEC2", "ISRC2DEC2"),
	MADERA_MUX_ROUTES("ISRC2DEC3", "ISRC2DEC3"),
	MADERA_MUX_ROUTES("ISRC2DEC4", "ISRC2DEC4"),

	MADERA_MUX_ROUTES("ISRC3INT1", "ISRC3INT1"),
	MADERA_MUX_ROUTES("ISRC3INT2", "ISRC3INT2"),

	MADERA_MUX_ROUTES("ISRC3DEC1", "ISRC3DEC1"),
	MADERA_MUX_ROUTES("ISRC3DEC2", "ISRC3DEC2"),

	MADERA_MUX_ROUTES("ISRC4INT1", "ISRC4INT1"),
	MADERA_MUX_ROUTES("ISRC4INT2", "ISRC4INT2"),

	MADERA_MUX_ROUTES("ISRC4DEC1", "ISRC4DEC1"),
	MADERA_MUX_ROUTES("ISRC4DEC2", "ISRC4DEC2"),

	{ "AEC1 Loopback", "HPOUT1L", "OUT1L" },
	{ "AEC1 Loopback", "HPOUT1R", "OUT1R" },
	{ "HPOUT1L", NULL, "OUT1L" },
	{ "HPOUT1R", NULL, "OUT1R" },

	{ "AEC1 Loopback", "HPOUT2L", "OUT2L" },
	{ "AEC1 Loopback", "HPOUT2R", "OUT2R" },
	{ "HPOUT2L", NULL, "OUT2L" },
	{ "HPOUT2R", NULL, "OUT2R" },

	{ "AEC1 Loopback", "HPOUT3L", "OUT3L" },
	{ "AEC1 Loopback", "HPOUT3R", "OUT3R" },
	{ "HPOUT3L", NULL, "OUT3L" },
	{ "HPOUT3R", NULL, "OUT3R" },

	{ "AEC1 Loopback", "SPKDAT1L", "OUT5L" },
	{ "AEC1 Loopback", "SPKDAT1R", "OUT5R" },
	{ "SPKDAT1L", NULL, "OUT5L" },
	{ "SPKDAT1R", NULL, "OUT5R" },
	{ "SPKDAT Capture", NULL, "SPKDAT1L" },
	{ "SPKDAT Capture", NULL, "SPKDAT1R" },

	CS47L90_RXANC_INPUT_ROUTES("RXANCL", "RXANCL"),
	CS47L90_RXANC_INPUT_ROUTES("RXANCR", "RXANCR"),

	CS47L90_RXANC_OUTPUT_ROUTES("OUT1L", "HPOUT1L"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT1R", "HPOUT1R"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT2L", "HPOUT2L"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT2R", "HPOUT2R"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT3L", "HPOUT3L"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT3R", "HPOUT3R"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT5L", "SPKDAT1L"),
	CS47L90_RXANC_OUTPUT_ROUTES("OUT5R", "SPKDAT1R"),

	{ "SPDIF1", NULL, "SPD1" },

	{ "MICSUPP", NULL, "SYSCLK" },

	{ "DRC1 Signal Activity", NULL, "DRC1 Activity Output" },
	{ "DRC2 Signal Activity", NULL, "DRC2 Activity Output" },
	{ "DRC1 Activity Output", "Switch", "DRC1L" },
	{ "DRC1 Activity Output", "Switch", "DRC1R" },
	{ "DRC2 Activity Output", "Switch", "DRC2L" },
	{ "DRC2 Activity Output", "Switch", "DRC2R" },

	MADERA_MUX_ROUTES("DFC1", "DFC1"),
	MADERA_MUX_ROUTES("DFC2", "DFC2"),
	MADERA_MUX_ROUTES("DFC3", "DFC3"),
	MADERA_MUX_ROUTES("DFC4", "DFC4"),
	MADERA_MUX_ROUTES("DFC5", "DFC5"),
	MADERA_MUX_ROUTES("DFC6", "DFC6"),
	MADERA_MUX_ROUTES("DFC7", "DFC7"),
	MADERA_MUX_ROUTES("DFC8", "DFC8"),
};

static int cs47l90_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			   unsigned int Fref, unsigned int Fout)
{
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case MADERA_FLL1_REFCLK:
		return madera_set_fll_refclk(&cs47l90->fll[0], source, Fref,
					     Fout);
	case MADERA_FLL2_REFCLK:
		return madera_set_fll_refclk(&cs47l90->fll[1], source, Fref,
					     Fout);
	case MADERA_FLLAO_REFCLK:
		return madera_set_fll_ao_refclk(&cs47l90->fll[2], source, Fref,
						Fout);
	case MADERA_FLL1_SYNCCLK:
		return madera_set_fll_syncclk(&cs47l90->fll[0], source, Fref,
					      Fout);
	case MADERA_FLL2_SYNCCLK:
		return madera_set_fll_syncclk(&cs47l90->fll[1], source, Fref,
					      Fout);
	default:
		return -EINVAL;
	}
}

static struct snd_soc_dai_driver cs47l90_dai[] = {
	{
		.name = "cs47l90-aif1",
		.id = 1,
		.base = MADERA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "cs47l90-aif2",
		.id = 2,
		.base = MADERA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "cs47l90-aif3",
		.id = 3,
		.base = MADERA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
/* slim1 id should be 4
	{
		.name = "cs47l90-aif4",
		.id = 4,
		.base = MADERA_AIF4_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF4 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF4 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_dai_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	},
*/
	{
		.name = "cs47l90-slim1",
		.id = 4,
		.playback = {
			.stream_name = "Slim1 Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim1 Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_slim_dai_ops,
	},
	{
		.name = "cs47l90-slim2",
		.id = 5,
		.playback = {
			.stream_name = "Slim2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MADERA_RATES,
			 .formats = MADERA_FORMATS,
		 },
		.ops = &madera_slim_dai_ops,
	},
	{
		.name = "cs47l90-pdm",
		.id = 8,
		.capture = {
			.stream_name = "SPKDAT Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
	{
		.name = "cs47l90-cpu-voicectrl",
		.capture = {
			.stream_name = "Voice Control CPU",
			.channels_min = 1,
			.channels_max = 1,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "cs47l90-dsp-voicectrl",
		.capture = {
			.stream_name = "Voice Control DSP",
			.channels_min = 1,
			.channels_max = 1,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
	{
		.name = "cs47l90-cpu-trace",
		.capture = {
			.stream_name = "Audio Trace CPU",
			.channels_min = 1,
			.channels_max = 6,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "cs47l90-dsp-trace",
		.capture = {
			.stream_name = "Audio Trace DSP",
			.channels_min = 1,
			.channels_max = 6,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
	{
		.name = "cs47l90-dsp1-cpu-txt",
		.capture = {
			.stream_name = "Text DSP1 CPU",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "cs47l90-dsp1-txt",
		.capture = {
			.stream_name = "Text DSP1",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
	{
		.name = "cs47l90-dsp2-cpu-txt",
		.capture = {
			.stream_name = "Text DSP2 CPU",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "cs47l90-dsp2-txt",
		.capture = {
			.stream_name = "Text DSP2",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
	{
		.name = "cs47l90-dsp3-cpu-txt",
		.capture = {
			.stream_name = "Text DSP3 CPU",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "cs47l90-dsp3-txt",
		.capture = {
			.stream_name = "Text DSP3",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MADERA_RATES,
			.formats = MADERA_FORMATS,
		},
	},
};

static int cs47l90_open(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(rtd->codec);
	struct madera_priv *priv = &cs47l90->core;
	struct madera *madera = priv->madera;
	int n_adsp, channel;

	dev_dbg(madera->dev,
			"Open compr stream '%s' for DAI %d '%s'\n",
			stream->name, rtd->codec_dai->id, rtd->codec_dai->name);

	if (strcmp(rtd->codec_dai->name, "cs47l90-dsp-voicectrl") == 0) {
	/* DSP 3 channel 1 */
		n_adsp = 2;
		channel = 0;
	} else if (strcmp(rtd->codec_dai->name, "cs47l90-dsp-trace") == 0) {
	/* DSP 1 channel 1 */
		n_adsp = 0;
		channel = 0;
	} else if (strcmp(rtd->codec_dai->name, "cs47l90-dsp1-txt") == 0) {
	/* DSP 1 channel 2 */
		n_adsp = 0;
		channel = 1;
	} else if (strcmp(rtd->codec_dai->name, "cs47l90-dsp3-txt") == 0) {
	/* DSP 3 channel 2 */
		n_adsp = 2;
		channel = 1;
	} else if (strcmp(rtd->codec_dai->name, "cs47l90-dsp2-txt") == 0) {
	/* DSP 2 channel 1 */
		n_adsp = 1;
		channel = 0;
	} else {
		dev_err(madera->dev,
			"No suitable compressed stream for DAI '%s'\n",
			rtd->codec_dai->name);
		return -EINVAL;
	}

	return wm_adsp_compr_open(&priv->adsp[n_adsp], stream, channel);
}

static int cs47l90_panic_check(struct cs47l90 *cs47l90, int dev, int *reg)
{
	struct madera_priv *priv = &cs47l90->core;
	struct madera *madera = priv->madera;
	unsigned int val, scratch1;
	struct madera_voice_trigger_info trig_info;

	*reg = MADERA_DSP1_SCRATCH_1 + (0x80000 * dev);
	regmap_read(madera->regmap_32bit, *reg, &val);

	val = val >> 16;
	if ((val & 0x3fff) == 0)
		return val;

	dev_err(madera->dev, "DSP%d Panic %x\n", dev, val);

	scratch1 = val;
	memset(trig_info.err_msg, 0, sizeof(trig_info.err_msg));

	regmap_read(madera->regmap_32bit, *reg, &val);
	trig_info.err_msg[1] = (u16)val;
	trig_info.err_msg[0] = (u16)(val >> 16);

	regmap_read(madera->regmap_32bit, *reg+2, &val);
	trig_info.err_msg[2] = (u16)val;
	trig_info.err_msg[3] = (u16)(val >> 16);


	/* Panic callback */
	trig_info.core_num = dev;
	trig_info.code = MADERA_TRIGGER_PANIC;
	madera_call_notifiers(madera,
			      MADERA_NOTIFY_VOICE_TRIGGER,
			      &trig_info);

	/* Clean panic field */
	scratch1 &= 0xc000;
	regmap_write(madera->regmap_32bit, *reg, (scratch1<<16));

	return scratch1;
}

static int cs47l90_text_event(struct cs47l90 *cs47l90, int dev)
{
	struct madera_priv *priv = &cs47l90->core;
	struct madera *madera = priv->madera;

	dev_dbg(madera->dev, "DSP%d Text event\n", dev);
	return 0;
}

static irqreturn_t cs47l90_adsp2_irq(int irq, void *data)
{
	struct cs47l90 *cs47l90 = data;
	struct madera_priv *priv = &cs47l90->core;
	struct madera *madera = priv->madera;
	struct madera_voice_trigger_info trig_info;
	int i, scratch_reg, reg, ret = 0;
	int serviced = 0, channel = 0;

	for (i = 0; i < CS47L90_NUM_ADSP; ++i) {
		scratch_reg = cs47l90_panic_check(cs47l90, i, &reg);
		dev_dbg(madera->dev, "dsp %d, scratch_reg %x\n", i, scratch_reg);
		if ((scratch_reg & 0x3fff) == 0) {
			if (scratch_reg & 0x4000) {
				/* Clear code bit */
				scratch_reg &= 0xbfff;
				scratch_reg = scratch_reg << 16;
				regmap_write(madera->regmap_32bit, reg,
					scratch_reg);
			trig_info.core_num = i;
			trig_info.code = MADERA_TRIGGER_VOICE;
			madera_call_notifiers(madera,
					      MADERA_NOTIFY_VOICE_TRIGGER,
					      &trig_info);
			channel = 0;
			dev_dbg(madera->dev, "Call AOV voice trigger notifier\n");
			}

			/* Text interrupt */
			if (scratch_reg & 0x8000) {
				/* Clear event bit */
				scratch_reg &= 0x7fff;
				scratch_reg = scratch_reg << 16;
				regmap_write(madera->regmap_32bit, reg,
					scratch_reg);
				dev_dbg(madera->dev, "Calling Text Callback\n");
				cs47l90_text_event(cs47l90, i);
				channel = 1;
			}
		}
		ret = wm_adsp_compr_handle_irq(&priv->adsp[i], channel);
		if (ret != -ENODEV)
			serviced++;
	}

	if (!serviced) {
		dev_err(madera->dev, "Spurious compressed data IRQ\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static irqreturn_t cs47l90_dsp_bus_error(int irq, void *data)
{
	struct wm_adsp *dsp = (struct wm_adsp *)data;

	return wm_adsp2_bus_error(dsp);
}

static const char * const cs47l90_dmic_refs[] = {
	"MICVDD",
	"MICBIAS1",
	"MICBIAS2",
	"MICBIAS3",
};

static const char * const cs47l90_dmic_inputs[] = {
	"IN1L Mux",
	"IN1R Mux",
	"IN2L Mux",
	"IN2R",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
	"IN5L",
	"IN5R",
};

static int cs47l90_codec_probe(struct snd_soc_codec *codec)
{
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);
	struct madera *madera = cs47l90->core.madera;
	int ret, i;

	cs47l90->core.madera->dapm = snd_soc_codec_get_dapm(codec);

	ret = madera_init_inputs(codec,
				 cs47l90_dmic_inputs,
				 ARRAY_SIZE(cs47l90_dmic_inputs),
				 cs47l90_dmic_refs,
				 ARRAY_SIZE(cs47l90_dmic_refs));
	if (ret)
		return ret;

	ret = madera_init_outputs(codec);
	if (ret)
		return ret;

	ret = madera_init_drc(codec);
	if (ret)
		return ret;

	snd_soc_dapm_disable_pin(madera->dapm, "HAPTICS");

	ret = snd_soc_add_codec_controls(codec, madera_adsp_rate_controls,
					 CS47L90_NUM_ADSP);
	if (ret)
		return ret;

	ret = madera_request_irq(madera, MADERA_IRQ_DSP_IRQ1,
				 "ADSP2 Compressed IRQ", cs47l90_adsp2_irq,
				 cs47l90);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to request DSP IRQ: %d\n", ret);
		return ret;
	}

	for (i = 0; i < CS47L90_NUM_ADSP; i++) {
		wm_adsp2_codec_probe(&cs47l90->core.adsp[i], codec);

		ret = madera_init_bus_error_irq(codec, i,
						cs47l90_dsp_bus_error);
		if (ret) {
			madera_free_irq(madera, MADERA_IRQ_DSP_IRQ1, cs47l90);
			wm_adsp2_codec_remove(&cs47l90->core.adsp[i], codec);

			for (--i; i >= 0; --i) {
				wm_adsp2_codec_remove(&cs47l90->core.adsp[i],
						      codec);
				madera_destroy_bus_error_irq(codec, i);
			}

			return ret;
		}
	}
#if IS_ENABLED(CONFIG_SND_SOC_AOV_TRIGGER)
	aov_trigger_register_notifier(codec);
#endif
	return 0;
}

static int cs47l90_codec_remove(struct snd_soc_codec *codec)
{
	int i;
	struct cs47l90 *cs47l90 = snd_soc_codec_get_drvdata(codec);
	struct madera *madera = cs47l90->core.madera;

	for (i = 0; i < CS47L90_NUM_ADSP; i++) {
		wm_adsp2_codec_remove(&cs47l90->core.adsp[i], codec);
		madera_destroy_bus_error_irq(codec, i);
	}

	madera_free_irq(madera, MADERA_IRQ_DSP_IRQ1, cs47l90);

	cs47l90->core.madera->dapm = NULL;

	return 0;
}

#define CS47L90_DIG_VU 0x0200

static unsigned int cs47l90_digital_vu[] = {
	MADERA_DAC_DIGITAL_VOLUME_1L,
	MADERA_DAC_DIGITAL_VOLUME_1R,
	MADERA_DAC_DIGITAL_VOLUME_2L,
	MADERA_DAC_DIGITAL_VOLUME_2R,
	MADERA_DAC_DIGITAL_VOLUME_3L,
	MADERA_DAC_DIGITAL_VOLUME_3R,
	MADERA_DAC_DIGITAL_VOLUME_5L,
	MADERA_DAC_DIGITAL_VOLUME_5R,
};

static struct regmap *cs47l90_get_regmap(struct device *dev)
{
	struct cs47l90 *cs47l90 = dev_get_drvdata(dev);

	return cs47l90->core.madera->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_cs47l90 = {
	.probe = cs47l90_codec_probe,
	.remove = cs47l90_codec_remove,
	.get_regmap = cs47l90_get_regmap,

	.idle_bias_off = true,

	.set_sysclk = madera_set_sysclk,
	.set_pll = cs47l90_set_fll,
	.component_driver = {
		.controls = cs47l90_snd_controls,
		.num_controls = ARRAY_SIZE(cs47l90_snd_controls),
		.dapm_widgets = cs47l90_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(cs47l90_dapm_widgets),
		.dapm_routes = cs47l90_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(cs47l90_dapm_routes),
	},
};

static struct snd_compr_ops cs47l90_compr_ops = {
	.open = cs47l90_open,
	.free = wm_adsp_compr_free,
	.set_params = wm_adsp_compr_set_params,
	.get_caps = wm_adsp_compr_get_caps,
	.trigger = wm_adsp_compr_trigger,
	.pointer = wm_adsp_compr_pointer,
	.copy = wm_adsp_compr_copy,
};

static struct snd_soc_platform_driver cs47l90_compr_platform = {
	.compr_ops = &cs47l90_compr_ops,
};

static int cs47l90_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	struct cs47l90 *cs47l90;
	int i, ret;

	BUILD_BUG_ON(ARRAY_SIZE(cs47l90_dai) > MADERA_MAX_DAI);

	/* FX Rate has the largest number of sources */
	BUILD_BUG_ON(ARRAY_SIZE(cs47l90->core.mixer_sources_cache) <
		     ARRAY_SIZE(cs47l90_fx_inputs));

	/* quick exit if Madera irqchip driver hasn't completed probe */
	if (!madera->irq_dev) {
		dev_dbg(&pdev->dev, "irqchip driver not ready\n");
		return -EPROBE_DEFER;
	}

	cs47l90 = devm_kzalloc(&pdev->dev, sizeof(struct cs47l90),
			      GFP_KERNEL);
	if (!cs47l90)
		return -ENOMEM;

	platform_set_drvdata(pdev, cs47l90);

	/* Set of_node to parent from the SPI device to allow DAPM to
	 * locate regulator supplies */
	pdev->dev.of_node = madera->dev->of_node;

	cs47l90->core.madera = madera;
	cs47l90->core.num_inputs = 10;
	cs47l90->core.get_sources = cs47l90_get_sources;

	ret = madera_core_init(&cs47l90->core);
	if (ret)
		return ret;

	for (i = 0; i < CS47L90_NUM_ADSP; i++) {
		cs47l90->core.adsp[i].part = "cs47l90";
		cs47l90->core.adsp[i].num = i + 1;
		cs47l90->core.adsp[i].type = WMFW_ADSP2;
		cs47l90->core.adsp[i].rev = 2;
		cs47l90->core.adsp[i].dev = madera->dev;
		cs47l90->core.adsp[i].regmap = madera->regmap_32bit;

		cs47l90->core.adsp[i].base = cs47l90_dsp_control_bases[i];
		cs47l90->core.adsp[i].mem = cs47l90_dsp_regions[i];
		cs47l90->core.adsp[i].num_mems
			= ARRAY_SIZE(cs47l90_dsp1_regions);

		cs47l90->core.adsp[i].lock_regions = WM_ADSP2_REGION_1_9;

		ret = wm_adsp2_init(&cs47l90->core.adsp[i]);
		if (ret != 0) {
			for (--i; i >= 0; --i)
				wm_adsp2_remove(&cs47l90->core.adsp[i]);
			goto error_core;
		}
	}

	madera_init_fll(madera, 1, MADERA_FLL1_CONTROL_1 - 1,
			&cs47l90->fll[0]);
	madera_init_fll(madera, 2, MADERA_FLL2_CONTROL_1 - 1,
			&cs47l90->fll[1]);
	madera_init_fll(madera, 4, MADERA_FLLAO_CONTROL_1 - 1,
			&cs47l90->fll[2]);

	for (i = 0; i < ARRAY_SIZE(cs47l90_dai); i++)
		madera_init_dai(&cs47l90->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(cs47l90_digital_vu); i++)
		regmap_update_bits(madera->regmap, cs47l90_digital_vu[i],
				   CS47L90_DIG_VU, CS47L90_DIG_VU);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	ret = snd_soc_register_platform(&pdev->dev, &cs47l90_compr_platform);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register platform: %d\n", ret);
		goto error;
	}

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_cs47l90,
				     cs47l90_dai, ARRAY_SIZE(cs47l90_dai));
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to register codec: %d\n", ret);
		snd_soc_unregister_platform(&pdev->dev);
		goto error;
	}

	return ret;

error:
	for (i = 0; i < CS47L90_NUM_ADSP; i++)
		wm_adsp2_remove(&cs47l90->core.adsp[i]);
error_core:
	madera_core_destroy(&cs47l90->core);

	return ret;
}

static int cs47l90_remove(struct platform_device *pdev)
{
	struct cs47l90 *cs47l90 = platform_get_drvdata(pdev);
	int i;

	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_codec(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	for (i = 0; i < CS47L90_NUM_ADSP; i++)
		wm_adsp2_remove(&cs47l90->core.adsp[i]);

	madera_core_destroy(&cs47l90->core);

	return 0;
}

static struct platform_driver cs47l90_codec_driver = {
	.driver = {
		.name = "cs47l90-codec",
	},
	.probe = cs47l90_probe,
	.remove = cs47l90_remove,
};

module_platform_driver(cs47l90_codec_driver);

MODULE_DESCRIPTION("ASoC CS47L90 driver");
MODULE_AUTHOR("Nikesh Oswal <nikesh@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cs47l90-codec");
