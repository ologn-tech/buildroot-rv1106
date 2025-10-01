################################################################################
#
# rv1106-rgbir-rtsp
#
################################################################################

RV1106_RGBIR_RTSP_SITE = package/rv1106-rgbir-rtsp/src
RV1106_RGBIR_RTSP_SITE_METHOD = local
RV1106_RGBIR_RTSP_LICENSE = GPL-2.0+
RV1106_RGBIR_RTSP_LICENSE_FILES = LICENSE

RV1106_RGBIR_RTSP_DEPENDENCIES = rv1106-ipc-sdk opencv4 libmpix

RV1106_RGBIR_RTSP_CONF_OPTS = \
	-DLIBMPIX_DIR=$(LIBMPIX_DIR) \
	-DOpenCV_DIR=$(STAGING_DIR)/usr/lib/cmake/opencv4 \
	-DRV1106_IPC_SDK_DIR=$(RV1106_IPC_SDK_DIR) \
	-DCMAKE_CXX_FLAGS="-DVIDEO_WIDTH=$(BR2_PACKAGE_RV1106_RGBIR_RTSP_WIDTH) -DVIDEO_HEIGHT=$(BR2_PACKAGE_RV1106_RGBIR_RTSP_HEIGHT)"

$(eval $(cmake-package))
