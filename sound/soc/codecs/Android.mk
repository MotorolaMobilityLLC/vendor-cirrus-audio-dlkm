DLKM_DIR := motorola/kernel/modules
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ifeq ($(CIRRUS_AMP_CODEC),cs35l41_i2c)
KERNEL_CFLAGS += CONFIG_SND_SOC_CS35L41_I2C=y
else
KERNEL_CFLAGS += CONFIG_SND_SOC_CS35L41_SPI=y
endif
LOCAL_MODULE := cirrus_cs35l41.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk

