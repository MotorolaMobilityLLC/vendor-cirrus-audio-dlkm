LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := cirrus_wm_adsp.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(TOP)/vendor/qcom/opensource/core-utils/dlkm/AndroidKernelModule.mk

include $(CLEAR_VARS)
ifeq ($(CIRRUS_AMP_CODEC),cs35l41_spi)
KERNEL_CFLAGS += CONFIG_SND_SOC_CS35L41_SPI=y
else
KERNEL_CFLAGS += CONFIG_SND_SOC_CS35L41_I2C=y
endif
LOCAL_MODULE := cirrus_cs35l41.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(TOP)/vendor/qcom/opensource/core-utils/dlkm/AndroidKernelModule.mk

