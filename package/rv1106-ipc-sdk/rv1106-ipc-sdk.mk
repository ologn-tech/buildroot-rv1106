################################################################################
#
# rv1106-ipc-sdk
#
################################################################################

RV1106_IPC_SDK_SITE = https://github.com/LuckfoxTECH/luckfox-pico.git
RV1106_IPC_SDK_SITE_METHOD = git
RV1106_IPC_SDK_VERSION = e2b0ffa22eb7cf158988d241b967a5583942d2ce

RV1106_IPC_SDK_INSTALL_IMAGES = YES
RV1106_IPC_SDK_INSTALL_TARGET = NO

define RV1106_IPC_SDK_INSTALL_IMAGES_CMDS
	$(INSTALL) -m 0755 -D $(@D)/tools/linux/Linux_Pack_Firmware/afptool $(HOST_DIR)/bin/afptool
	$(INSTALL) -m 0755 -D $(@D)/tools/linux/Linux_Pack_Firmware/mk-update_pack.sh $(HOST_DIR)/bin/mk-update_pack.sh
	$(INSTALL) -m 0755 -D $(@D)/tools/linux/Linux_Pack_Firmware/rkImageMaker $(HOST_DIR)/bin/rkImageMaker
endef

$(eval $(generic-package))
