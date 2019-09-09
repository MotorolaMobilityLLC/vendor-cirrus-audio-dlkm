DLKM_DIR := motorola/kernel/modules
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ifeq ($(CIRRUS_CODEC),cs47l35)
KERNEL_CFLAGS += CONFIG_SND_SOC_CS47L35=y
else
KERNEL_CFLAGS += CONFIG_SND_SOC_CS47L90=y
endif
LOCAL_MODULE := cirrus_irq-madera.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk

