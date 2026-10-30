################################################################################
#
# rv1106-smartir-rtsp
#
################################################################################

RV1106_SMARTIR_RTSP_SITE = package/rv1106-smartir-rtsp/src
RV1106_SMARTIR_RTSP_SITE_METHOD = local
RV1106_SMARTIR_RTSP_LICENSE = GPL-2.0+
RV1106_SMARTIR_RTSP_LICENSE_FILES = LICENSE

RV1106_SMARTIR_RTSP_DEPENDENCIES = rv1106-ipc-sdk

RV1106_SMARTIR_RTSP_CONF_OPTS = \
	-DRV1106_IPC_SDK_DIR=$(RV1106_IPC_SDK_DIR) \
	-DWIDTH=$(BR2_PACKAGE_RV1106_SMARTIR_RTSP_WIDTH) \
	-DHEIGHT=$(BR2_PACKAGE_RV1106_SMARTIR_RTSP_HEIGHT)

$(eval $(cmake-package))
