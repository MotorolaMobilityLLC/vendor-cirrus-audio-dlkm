// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * cs35l45-tables.c -- CS35L45 ALSA SoC audio driver
 *
 * Copyright 2019 Cirrus Logic, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */

#include "cs35l45.h"

const struct reg_default cs35l45_reg[CS35L45_MAX_CACHE_REG] = {
	{CS35L45_GLOBAL_ENABLES,		0x00000000},
	{CS35L45_BLOCK_ENABLES,			0x00003323},
	{CS35L45_BLOCK_ENABLES2,		0x00000010},
	{CS35L45_GLOBAL_OVERRIDES,		0x00000002},
	{CS35L45_GLOBAL_SYNC,			0x00000000},
	{CS35L45_ERROR_RELEASE,			0x00000000},
	{CS35L45_SYNC_GPIO1,			0x00000007},
	{CS35L45_INTB_GPIO2_MCLK_REF,		0x00000005},
	{CS35L45_GPIO3,				0x00000005},
	{CS35L45_GPIO_GLOBAL_ENABLE_CONTROL,	0x00000000},
	{CS35L45_PWRMGT_CTL,			0x00000000},
	{CS35L45_WAKESRC_CTL,			0x00000008},
	{CS35L45_WKI2C_CTL,			0x00000030},
	{CS35L45_PWRMGT_STS,			0x00000000},
	{CS35L45_REFCLK_INPUT,			0x00000510},
	{CS35L45_GLOBAL_SAMPLE_RATE,		0x00000003},
	{CS35L45_SWIRE_CLK_CTRL,		0x00000000},
	{CS35L45_SYNC_TX_RX_ENABLES,		0x00000200},
	{CS35L45_BOOST_VOLTAGE_CFG,		0x000001BE},
	{CS35L45_BOOST_CCM_CFG,			0xF0000001},
	{CS35L45_BOOST_DCM_CFG,			0x08710200},
	{CS35L45_BOOST_LPMODE_CFG,		0x00000002},
	{CS35L45_BOOST_RAMP_CFG,		0x0000004A},
	{CS35L45_BOOST_STARTUP_CFG,		0x0000831D},
	{CS35L45_BOOST_OV_CFG,			0x005400D0},
	{CS35L45_BOOST_UV_CFG,			0x00000570},
	{CS35L45_BOOST_STATUS,			0x00000001},
	{CS35L45_BST_BPE_INST_THLD,		0x5A46321E},
	{CS35L45_BST_BPE_INST_ILIM,		0x3C140C04},
	{CS35L45_BST_BPE_INST_SS_ILIM,		0x1C080400},
	{CS35L45_BST_BPE_INST_ATK_RATE,		0x06060600},
	{CS35L45_BST_BPE_INST_HOLD_TIME,	0x02020202},
	{CS35L45_BST_BPE_INST_RLS_RATE,		0x06060606},
	{CS35L45_BST_BPE_MISC_CONFIG,		0x00000000},
	{CS35L45_BST_BPE_IL_LIM_THLD,		0x0006022C},
	{CS35L45_BST_BPE_IL_LIM_DLY,		0x0000040C},
	{CS35L45_BST_BPE_IL_LIM_ATK_RATE,	0x00000000},
	{CS35L45_BST_BPE_IL_LIM_RLS_RATE,	0x00000000},
	{CS35L45_BST_BPE_INST_STATUS,		0x0000005A},
	{CS35L45_MONITOR_FILT,			0x00000000},
	{CS35L45_IMON_COMP,			0x00000036},
	{CS35L45_STATUS,			0x00000010},
	{CS35L45_MON_VALUE,			0x00000000},
	{CS35L45_ASP_ENABLES1,			0x00000000},
	{CS35L45_ASP_CONTROL1,			0x00000028},
	{CS35L45_ASP_CONTROL2,			0x18180200},
	{CS35L45_ASP_CONTROL3,			0x00000002},
	{CS35L45_ASP_FRAME_CONTROL1,		0x03020100},
	{CS35L45_ASP_FRAME_CONTROL2,		0x00000004},
	{CS35L45_ASP_FRAME_CONTROL5,		0x00000100},
	{CS35L45_ASP_DATA_CONTROL1,		0x00000018},
	{CS35L45_ASP_DATA_CONTROL5,		0x00000018},
	{CS35L45_DACPCM1_INPUT,			0x00000008},
	{CS35L45_ASPTX1_INPUT,			0x00000018},
	{CS35L45_ASPTX2_INPUT,			0x00000019},
	{CS35L45_ASPTX3_INPUT,			0x00000020},
	{CS35L45_ASPTX4_INPUT,			0x00000021},
	{CS35L45_ASPTX5_INPUT,			0x00000048},
	{CS35L45_DSP1RX1_INPUT,			0x00000008},
	{CS35L45_DSP1RX2_INPUT,			0x00000009},
	{CS35L45_DSP1RX3_INPUT,			0x00000018},
	{CS35L45_DSP1RX4_INPUT,			0x00000019},
	{CS35L45_DSP1RX5_INPUT,			0x00000020},
	{CS35L45_DSP1RX6_INPUT,			0x00000021},
	{CS35L45_DSP1RX7_INPUT,			0x0000003A},
	{CS35L45_DSP1RX8_INPUT,			0x00000028},
	{CS35L45_NGATE1_INPUT,			0x00000008},
	{CS35L45_NGATE2_INPUT,			0x00000009},
	{CS35L45_SWIRE_PORT1_CH1_INPUT,		0x00000018},
	{CS35L45_SWIRE_PORT1_CH2_INPUT,		0x00000019},
	{CS35L45_SWIRE_PORT1_CH3_INPUT,		0x00000020},
	{CS35L45_SWIRE_PORT1_CH4_INPUT,		0x00000021},
	{CS35L45_SWIRE_PORT1_CH5_INPUT,		0x00000048},
	{CS35L45_AMP_ERR_VOL_SEL,		0x00000001},
	{CS35L45_TEMP_WARN_THRESHOLD,		0x00000003},
	{CS35L45_TEMP_WARN_CONFIG,		0x00522183},
	{CS35L45_TEMP_WARN_TRIG_AUTO,		0x00000010},
	{CS35L45_TEMP_WARN_STATUS,		0x00000000},
	{CS35L45_BPE_INST_THLD,			0x5A46321E},
	{CS35L45_BPE_INST_ATTN,			0x060C1218},
	{CS35L45_BPE_INST_ATK_RATE,		0x06060606},
	{CS35L45_BPE_INST_HOLD_TIME,		0x02020202},
	{CS35L45_BPE_INST_RLS_RATE,		0x05050505},
	{CS35L45_BPE_MISC_CONFIG,		0x00000000},
	{CS35L45_BPE_INST_STATUS,		0x0000005A},
	{CS35L45_HVLV_CONFIG,			0x00440017},
	{CS35L45_LDPM_CONFIG,			0x00013636},
	{CS35L45_CLASSH_CONFIG1,		0x02000B04},
	{CS35L45_CLASSH_CONFIG2,		0x009600FA},
	{CS35L45_CLASSH_CONFIG3,		0x00000000},
	{CS35L45_AUD_MEM,			0x00000007},
	{CS35L45_AMP_PCM_CONTROL,		0x00100000},
	{CS35L45_AMP_GAIN,			0x00002300},
	{CS35L45_DAC_MSM_CONFIG,		0x00000020},
	{CS35L45_AMP_OUTPUT_MUTE,		0x00000000},
	{CS35L45_AMP_OUTPUT_DRV,		0x00000040},
	{CS35L45_ALIVE_DCIN_WD,			0x00000263},
	{CS35L45_IRQ1_CFG,			0x00000000},
	{CS35L45_IRQ2_CFG,			0x00000000},
	{CS35L45_GPIO1_CTRL1,			0x81000001},
	{CS35L45_GPIO2_CTRL1,			0x81000001},
	{CS35L45_GPIO3_CTRL1,			0x81000001},
	{CS35L45_MIXER_NGATE_CH1_CFG,		0x00000303},
	{CS35L45_MIXER_NGATE_CH2_CFG,		0x00000303},
	{CS35L45_CLOCK_DETECT_1,		0x00000030},
};

bool cs35l45_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L45_DEVID:
	case CS35L45_REVID:
	case CS35L45_RELID:
	case CS35L45_OTPID:
	case CS35L45_SFT_RESET:
	case CS35L45_GLOBAL_ENABLES:
	case CS35L45_BLOCK_ENABLES:
	case CS35L45_BLOCK_ENABLES2:
	case CS35L45_GLOBAL_OVERRIDES:
	case CS35L45_GLOBAL_SYNC:
	case CS35L45_ERROR_RELEASE:
	case CS35L45_SYNC_GPIO1:
	case CS35L45_INTB_GPIO2_MCLK_REF:
	case CS35L45_GPIO3:
	case CS35L45_GPIO_GLOBAL_ENABLE_CONTROL:
	case CS35L45_PWRMGT_CTL:
	case CS35L45_WAKESRC_CTL:
	case CS35L45_WKI2C_CTL:
	case CS35L45_PWRMGT_STS:
	case CS35L45_REFCLK_INPUT:
	case CS35L45_GLOBAL_SAMPLE_RATE:
	case CS35L45_SWIRE_CLK_CTRL:
	case CS35L45_SYNC_TX_RX_ENABLES:
	case CS35L45_BOOST_VOLTAGE_CFG:
	case CS35L45_BOOST_CCM_CFG:
	case CS35L45_BOOST_DCM_CFG:
	case CS35L45_BOOST_LPMODE_CFG:
	case CS35L45_BOOST_RAMP_CFG:
	case CS35L45_BOOST_STARTUP_CFG:
	case CS35L45_BOOST_OV_CFG:
	case CS35L45_BOOST_UV_CFG:
	case CS35L45_BOOST_STATUS:
	case CS35L45_BST_BPE_INST_THLD:
	case CS35L45_BST_BPE_INST_ILIM:
	case CS35L45_BST_BPE_INST_SS_ILIM:
	case CS35L45_BST_BPE_INST_ATK_RATE:
	case CS35L45_BST_BPE_INST_HOLD_TIME:
	case CS35L45_BST_BPE_INST_RLS_RATE:
	case CS35L45_BST_BPE_MISC_CONFIG:
	case CS35L45_BST_BPE_IL_LIM_THLD:
	case CS35L45_BST_BPE_IL_LIM_DLY:
	case CS35L45_BST_BPE_IL_LIM_ATK_RATE:
	case CS35L45_BST_BPE_IL_LIM_RLS_RATE:
	case CS35L45_BST_BPE_INST_STATUS:
	case CS35L45_MONITOR_FILT:
	case CS35L45_IMON_COMP:
	case CS35L45_STATUS:
	case CS35L45_MON_VALUE:
	case CS35L45_ASP_ENABLES1:
	case CS35L45_ASP_CONTROL1:
	case CS35L45_ASP_CONTROL2:
	case CS35L45_ASP_CONTROL3:
	case CS35L45_ASP_FRAME_CONTROL1:
	case CS35L45_ASP_FRAME_CONTROL2:
	case CS35L45_ASP_FRAME_CONTROL5:
	case CS35L45_ASP_DATA_CONTROL1:
	case CS35L45_ASP_DATA_CONTROL5:
	case CS35L45_DACPCM1_INPUT:
	case CS35L45_MIXER_PILOT0_INPUT:
	case CS35L45_ASPTX1_INPUT:
	case CS35L45_ASPTX2_INPUT:
	case CS35L45_ASPTX3_INPUT:
	case CS35L45_ASPTX4_INPUT:
	case CS35L45_ASPTX5_INPUT:
	case CS35L45_DSP1RX1_INPUT:
	case CS35L45_DSP1RX2_INPUT:
	case CS35L45_DSP1RX3_INPUT:
	case CS35L45_DSP1RX4_INPUT:
	case CS35L45_DSP1RX5_INPUT:
	case CS35L45_DSP1RX6_INPUT:
	case CS35L45_DSP1RX7_INPUT:
	case CS35L45_DSP1RX8_INPUT:
	case CS35L45_NGATE1_INPUT:
	case CS35L45_NGATE2_INPUT:
	case CS35L45_SWIRE_PORT1_CH1_INPUT:
	case CS35L45_SWIRE_PORT1_CH2_INPUT:
	case CS35L45_SWIRE_PORT1_CH3_INPUT:
	case CS35L45_SWIRE_PORT1_CH4_INPUT:
	case CS35L45_SWIRE_PORT1_CH5_INPUT:
	case CS35L45_AMP_ERR_VOL_SEL:
	case CS35L45_TEMP_WARN_THRESHOLD:
	case CS35L45_TEMP_WARN_CONFIG:
	case CS35L45_TEMP_WARN_TRIG_AUTO:
	case CS35L45_TEMP_WARN_STATUS:
	case CS35L45_BPE_INST_THLD:
	case CS35L45_BPE_INST_ATTN:
	case CS35L45_BPE_INST_ATK_RATE:
	case CS35L45_BPE_INST_HOLD_TIME:
	case CS35L45_BPE_INST_RLS_RATE:
	case CS35L45_BPE_MISC_CONFIG:
	case CS35L45_BPE_INST_STATUS:
	case CS35L45_HVLV_CONFIG:
	case CS35L45_LDPM_CONFIG:
	case CS35L45_CLASSH_CONFIG1:
	case CS35L45_CLASSH_CONFIG2:
	case CS35L45_CLASSH_CONFIG3:
	case CS35L45_AUD_MEM:
	case CS35L45_AMP_PCM_CONTROL:
	case CS35L45_AMP_GAIN:
	case CS35L45_DAC_MSM_CONFIG:
	case CS35L45_AMP_OUTPUT_MUTE:
	case CS35L45_AMP_OUTPUT_DRV:
	case CS35L45_ALIVE_DCIN_WD:
	case CS35L45_IRQ1_CFG:
	case CS35L45_IRQ1_STATUS:
	case CS35L45_IRQ1_EINT_1:
	case CS35L45_IRQ1_EINT_2:
	case CS35L45_IRQ1_EINT_3:
	case CS35L45_IRQ1_EINT_4:
	case CS35L45_IRQ1_EINT_5:
	case CS35L45_IRQ1_EINT_7:
	case CS35L45_IRQ1_EINT_8:
	case CS35L45_IRQ1_EINT_18:
	case CS35L45_IRQ1_STS_1:
	case CS35L45_IRQ1_STS_2:
	case CS35L45_IRQ1_STS_3:
	case CS35L45_IRQ1_STS_4:
	case CS35L45_IRQ1_STS_5:
	case CS35L45_IRQ1_STS_7:
	case CS35L45_IRQ1_STS_8:
	case CS35L45_IRQ1_STS_18:
	case CS35L45_IRQ1_MASK_1:
	case CS35L45_IRQ1_MASK_2:
	case CS35L45_IRQ1_MASK_3:
	case CS35L45_IRQ1_MASK_4:
	case CS35L45_IRQ1_MASK_5:
	case CS35L45_IRQ1_MASK_7:
	case CS35L45_IRQ1_MASK_8:
	case CS35L45_IRQ1_MASK_18:
	case CS35L45_IRQ1_EDGE_1:
	case CS35L45_IRQ1_EDGE_4:
	case CS35L45_IRQ1_POL_1:
	case CS35L45_IRQ1_POL_2:
	case CS35L45_IRQ1_POL_4:
	case CS35L45_IRQ1_DB_3:
	case CS35L45_IRQ2_CFG:
	case CS35L45_IRQ2_STATUS:
	case CS35L45_IRQ2_EINT_1:
	case CS35L45_IRQ2_EINT_2:
	case CS35L45_IRQ2_EINT_3:
	case CS35L45_IRQ2_EINT_4:
	case CS35L45_IRQ2_EINT_5:
	case CS35L45_IRQ2_EINT_7:
	case CS35L45_IRQ2_EINT_8:
	case CS35L45_IRQ2_EINT_18:
	case CS35L45_IRQ2_STS_1:
	case CS35L45_IRQ2_STS_2:
	case CS35L45_IRQ2_STS_3:
	case CS35L45_IRQ2_STS_4:
	case CS35L45_IRQ2_STS_5:
	case CS35L45_IRQ2_STS_7:
	case CS35L45_IRQ2_STS_8:
	case CS35L45_IRQ2_STS_18:
	case CS35L45_IRQ2_MASK_1:
	case CS35L45_IRQ2_MASK_2:
	case CS35L45_IRQ2_MASK_3:
	case CS35L45_IRQ2_MASK_4:
	case CS35L45_IRQ2_MASK_5:
	case CS35L45_IRQ2_MASK_7:
	case CS35L45_IRQ2_MASK_8:
	case CS35L45_IRQ2_MASK_18:
	case CS35L45_IRQ2_EDGE_1:
	case CS35L45_IRQ2_EDGE_4:
	case CS35L45_IRQ2_POL_1:
	case CS35L45_IRQ2_POL_2:
	case CS35L45_IRQ2_POL_4:
	case CS35L45_IRQ2_DB_3:
	case CS35L45_GPIO_STATUS1:
	case CS35L45_GPIO1_CTRL1:
	case CS35L45_GPIO2_CTRL1:
	case CS35L45_GPIO3_CTRL1:
	case CS35L45_MIXER_NGATE_CH1_CFG:
	case CS35L45_MIXER_NGATE_CH2_CFG:
	case CS35L45_DSP_MBOX_1:
	case CS35L45_DSP_MBOX_2:
	case CS35L45_DSP_MBOX_3:
	case CS35L45_DSP_MBOX_4:
	case CS35L45_DSP_MBOX_5:
	case CS35L45_DSP_MBOX_6:
	case CS35L45_DSP_MBOX_7:
	case CS35L45_DSP_MBOX_8:
	case CS35L45_DSP_VIRT1_MBOX_1:
	case CS35L45_DSP_VIRT1_MBOX_2:
	case CS35L45_DSP_VIRT1_MBOX_3:
	case CS35L45_DSP_VIRT1_MBOX_4:
	case CS35L45_DSP_VIRT1_MBOX_5:
	case CS35L45_DSP_VIRT1_MBOX_6:
	case CS35L45_DSP_VIRT1_MBOX_7:
	case CS35L45_DSP_VIRT1_MBOX_8:
	case CS35L45_DSP_VIRT2_MBOX_1:
	case CS35L45_DSP_VIRT2_MBOX_2:
	case CS35L45_DSP_VIRT2_MBOX_3:
	case CS35L45_DSP_VIRT2_MBOX_4:
	case CS35L45_DSP_VIRT2_MBOX_5:
	case CS35L45_DSP_VIRT2_MBOX_6:
	case CS35L45_DSP_VIRT2_MBOX_7:
	case CS35L45_DSP_VIRT2_MBOX_8:
	case CS35L45_CLOCK_DETECT_1:
	case CS35L45_DSP1_SYS_ID:
	case CS35L45_DSP1_CLOCK_FREQ:
	case CS35L45_DSP1_RX1_RATE:
	case CS35L45_DSP1_RX2_RATE:
	case CS35L45_DSP1_RX3_RATE:
	case CS35L45_DSP1_RX4_RATE:
	case CS35L45_DSP1_RX5_RATE:
	case CS35L45_DSP1_RX6_RATE:
	case CS35L45_DSP1_RX7_RATE:
	case CS35L45_DSP1_RX8_RATE:
	case CS35L45_DSP1_TX1_RATE:
	case CS35L45_DSP1_TX2_RATE:
	case CS35L45_DSP1_TX3_RATE:
	case CS35L45_DSP1_TX4_RATE:
	case CS35L45_DSP1_TX5_RATE:
	case CS35L45_DSP1_TX6_RATE:
	case CS35L45_DSP1_TX7_RATE:
	case CS35L45_DSP1_TX8_RATE:
	case CS35L45_DSP1_SCRATCH1:
	case CS35L45_DSP1_SCRATCH2:
	case CS35L45_DSP1_SCRATCH3:
	case CS35L45_DSP1_SCRATCH4:
	case CS35L45_DSP1_CCM_CORE_CONTROL:
	case CS35L45_DSP1_STREAM_ARB_MSTR1_CONFIG_0:
	case CS35L45_DSP1_STREAM_ARB_TX1_CONFIG_0:
	case CS35L45_DSP1_XMEM_PACK_0 ... CS35L45_DSP1_XMEM_PACK_4607:
	case CS35L45_DSP1_XMEM_UNPACK32_0 ... CS35L45_DSP1_XMEM_UNPACK32_3071:
	case CS35L45_DSP1_XMEM_UNPACK24_0 ... CS35L45_DSP1_XMEM_UNPACK24_6143:
	case CS35L45_DSP1_YMEM_PACK_0 ... CS35L45_DSP1_YMEM_PACK_1532:
	case CS35L45_DSP1_YMEM_UNPACK32_0 ... CS35L45_DSP1_YMEM_UNPACK32_1022:
	case CS35L45_DSP1_YMEM_UNPACK24_0 ... CS35L45_DSP1_YMEM_UNPACK24_2043:
	case CS35L45_DSP1_PMEM_0 ... CS35L45_DSP1_PMEM_3834:
		return true;
	default:
		return false;
	}
}

bool cs35l45_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L45_DEVID:
	case CS35L45_SFT_RESET:
	case CS35L45_REVID:
	case CS35L45_PWRMGT_STS:
	case CS35L45_BOOST_STATUS:
	case CS35L45_BST_BPE_INST_STATUS:
	case CS35L45_STATUS:
	case CS35L45_MON_VALUE:
	case CS35L45_IRQ1_STATUS:
	case CS35L45_IRQ1_EINT_1:
	case CS35L45_IRQ1_EINT_2:
	case CS35L45_IRQ1_EINT_3:
	case CS35L45_IRQ1_EINT_4:
	case CS35L45_IRQ1_EINT_5:
	case CS35L45_IRQ1_EINT_7:
	case CS35L45_IRQ1_EINT_8:
	case CS35L45_IRQ1_EINT_18:
	case CS35L45_IRQ1_STS_1:
	case CS35L45_IRQ1_STS_2:
	case CS35L45_IRQ1_STS_3:
	case CS35L45_IRQ1_STS_4:
	case CS35L45_IRQ1_STS_5:
	case CS35L45_IRQ1_STS_7:
	case CS35L45_IRQ1_STS_8:
	case CS35L45_IRQ1_STS_18:
	case CS35L45_IRQ2_STATUS:
	case CS35L45_IRQ2_EINT_1:
	case CS35L45_IRQ2_EINT_2:
	case CS35L45_IRQ2_EINT_3:
	case CS35L45_IRQ2_EINT_4:
	case CS35L45_IRQ2_EINT_5:
	case CS35L45_IRQ2_EINT_7:
	case CS35L45_IRQ2_EINT_8:
	case CS35L45_IRQ2_EINT_18:
	case CS35L45_IRQ2_STS_1:
	case CS35L45_IRQ2_STS_2:
	case CS35L45_IRQ2_STS_3:
	case CS35L45_IRQ2_STS_4:
	case CS35L45_IRQ2_STS_5:
	case CS35L45_IRQ2_STS_7:
	case CS35L45_IRQ2_STS_8:
	case CS35L45_IRQ2_STS_18:
	case CS35L45_GPIO_STATUS1:
	case CS35L45_DSP_MBOX_1:
	case CS35L45_DSP_MBOX_2:
	case CS35L45_DSP_MBOX_3:
	case CS35L45_DSP_MBOX_4:
	case CS35L45_DSP_MBOX_5:
	case CS35L45_DSP_MBOX_6:
	case CS35L45_DSP_MBOX_7:
	case CS35L45_DSP_MBOX_8:
	case CS35L45_DSP_VIRT1_MBOX_1:
	case CS35L45_DSP_VIRT1_MBOX_2:
	case CS35L45_DSP_VIRT1_MBOX_3:
	case CS35L45_DSP_VIRT1_MBOX_4:
	case CS35L45_DSP_VIRT1_MBOX_5:
	case CS35L45_DSP_VIRT1_MBOX_6:
	case CS35L45_DSP_VIRT1_MBOX_7:
	case CS35L45_DSP_VIRT1_MBOX_8:
	case CS35L45_DSP_VIRT2_MBOX_1:
	case CS35L45_DSP_VIRT2_MBOX_2:
	case CS35L45_DSP_VIRT2_MBOX_3:
	case CS35L45_DSP_VIRT2_MBOX_4:
	case CS35L45_DSP_VIRT2_MBOX_5:
	case CS35L45_DSP_VIRT2_MBOX_6:
	case CS35L45_DSP_VIRT2_MBOX_7:
	case CS35L45_DSP_VIRT2_MBOX_8:
	case CS35L45_DSP1_SCRATCH1:
	case CS35L45_DSP1_SCRATCH2:
	case CS35L45_DSP1_SCRATCH3:
	case CS35L45_DSP1_SCRATCH4:
	case CS35L45_DSP1_XMEM_PACK_0 ... CS35L45_DSP1_XMEM_PACK_4607:
	case CS35L45_DSP1_XMEM_UNPACK32_0 ... CS35L45_DSP1_XMEM_UNPACK32_3071:
	case CS35L45_DSP1_XMEM_UNPACK24_0 ... CS35L45_DSP1_XMEM_UNPACK24_6143:
	case CS35L45_DSP1_YMEM_PACK_0 ... CS35L45_DSP1_YMEM_PACK_1532:
	case CS35L45_DSP1_YMEM_UNPACK32_0 ... CS35L45_DSP1_YMEM_UNPACK32_1022:
	case CS35L45_DSP1_YMEM_UNPACK24_0 ... CS35L45_DSP1_YMEM_UNPACK24_2043:
	case CS35L45_DSP1_PMEM_0 ... CS35L45_DSP1_PMEM_3834:
		return true;
	default:
		return false;
	}
}

const struct cs35l45_pll_sysclk_config
		cs35l45_pll_sysclk[CS35L45_MAX_PLL_CONFIGS] = {
	{ 32768,	0x00 },
	{ 8000,		0x01 },
	{ 11025,	0x02 },
	{ 12000,	0x03 },
	{ 16000,	0x04 },
	{ 22050,	0x05 },
	{ 24000,	0x06 },
	{ 32000,	0x07 },
	{ 44100,	0x08 },
	{ 48000,	0x09 },
	{ 88200,	0x0A },
	{ 96000,	0x0B },
	{ 128000,	0x0C },
	{ 176400,	0x0D },
	{ 192000,	0x0E },
	{ 256000,	0x0F },
	{ 352800,	0x10 },
	{ 384000,	0x11 },
	{ 512000,	0x12 },
	{ 705600,	0x13 },
	{ 750000,	0x14 },
	{ 768000,	0x15 },
	{ 1000000,	0x16 },
	{ 1024000,	0x17 },
	{ 1200000,	0x18 },
	{ 1411200,	0x19 },
	{ 1500000,	0x1A },
	{ 1536000,	0x1B },
	{ 2000000,	0x1C },
	{ 2048000,	0x1D },
	{ 2400000,	0x1E },
	{ 2822400,	0x1F },
	{ 3000000,	0x20 },
	{ 3072000,	0x21 },
	{ 3200000,	0x22 },
	{ 4000000,	0x23 },
	{ 4096000,	0x24 },
	{ 4800000,	0x25 },
	{ 5644800,	0x26 },
	{ 6000000,	0x27 },
	{ 6144000,	0x28 },
	{ 6250000,	0x29 },
	{ 6400000,	0x2A },
	{ 6500000,	0x2B },
	{ 6750000,	0x2C },
	{ 7526400,	0x2D },
	{ 8000000,	0x2E },
	{ 8192000,	0x2F },
	{ 9600000,	0x30 },
	{ 11289600,	0x31 },
	{ 12000000,	0x32 },
	{ 12288000,	0x33 },
	{ 12500000,	0x34 },
	{ 12800000,	0x35 },
	{ 13000000,	0x36 },
	{ 13500000,	0x37 },
	{ 19200000,	0x38 },
	{ 22579200,	0x39 },
	{ 24000000,	0x3A },
	{ 24576000,	0x3B },
	{ 25000000,	0x3C },
	{ 25600000,	0x3D },
	{ 26000000,	0x3E },
	{ 27000000,	0x3F },
};
