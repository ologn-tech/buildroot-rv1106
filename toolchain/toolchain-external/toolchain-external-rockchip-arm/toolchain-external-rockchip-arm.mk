################################################################################
#
# toolchain-external-rockchip-arm
#
################################################################################

TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_SITE = $(RV1106_IPC_SDK_DL_DIR)/git/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf
TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_SITE_METHOD = local

TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_PRE_CONFIGURE_HOOKS += TOOLCHAIN_EXTERNAL_MOVE

TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_DEPENDENCIES = rv1106-ipc-sdk

$(eval $(toolchain-external-package))
