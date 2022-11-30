hide := @
ECHO := echo

SDK_ROOT = /home/marc/ebat2.25sdk/sdk20210512

GCC := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
G++ := $(SDK_ROOT)/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++

SYSROOT = $(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/host/arm-buildroot-linux-gnueabihf/sysroot

CFLAGS := -I../../include/rkmedia \
		-I$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rknpu-1.5.0/rknn/rknn_api/librknn_api/include/ \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/common \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/xcore \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/uAPI \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/algos \
		-I$(SDK_ROOT)/external/camera_engine_rkaiq/include/iq_parser \
		-I./include \
		-I/usr/include/ \
		-I../common/ \
		-I/usr/arm-linux-gnueabihf/include/ \
		-I../common/include \
		-I$(SYSROOT)/usr/include/rknn/ 

LIB_FILES := -L$(SYSROOT)/usr/lib \
			 -L$(SDK_ROOT)/external/rkmedia/examples/librtsp/\
			 -L$(SDK_ROOT)/buildroot/output/rockchip_rv1126_rv1109/build/rockx/sdk/rockx-rk1806-Linux/lib/

LD_FLAGS := -lpthread -leasymedia -ldrm -lrockchip_mpp \
			-lasound -lv4l2 -lv4lconvert -lrga \
			-lRKAP_ANR -lRKAP_Common -lRKAP_3A \
			-lmd_share -lrkaiq -lod_share -lrtsp -lrknn_api
			# -lavformat -lavcodec -lswresample -lavutil \

CFLAGS += -DRKAIQ

SAMPLE_COMMON := ../common/sample_common_isp.c \
			     ../common/common.c

all:
	$(GCC) main.c $(SAMPLE_COMMON) $(LIB_FILES) $(LD_FLAGS) $(CFLAGS) -o build/double_output --sysroot=$(SYSROOT)
	$(hide)$(ECHO) "Build Done ..."

