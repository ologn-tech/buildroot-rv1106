################################################################################
#
# toolchain-external-rockchip-arm
#
################################################################################

TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_SITE = https://github.com/LuckfoxTECH/luckfox-pico.git
TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_SITE_METHOD = git
TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_VERSION = e1caff31f64cbb1e7dbed793b764bbf1c020d6c7

define TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_EXTRACT_CMDS
	cp -rf $(TOOLCHAIN_EXTERNAL_ROCKCHIP_ARM_DL_DIR)/git/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/* $(@D)
endef

$(eval $(toolchain-external-package))
