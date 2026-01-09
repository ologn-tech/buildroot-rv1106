################################################################################
#
# luckfox-pico-rtsp-opencv
#
################################################################################

LUCKFOX_PICO_RTSP_OPENCV_SITE = https://github.com/ologn-tech/luckfox_pico_rkmpi_example
LUCKFOX_PICO_RTSP_OPENCV_SITE_METHOD = git
LUCKFOX_PICO_RTSP_OPENCV_VERSION = 7d738cf4ae7b29543eab38dd5c1cb97527456a6c

LUCKFOX_PICO_RTSP_OPENCV_DEPENDENCIES = opencv4

LUCKFOX_PICO_RTSP_OPENCV_CONF_OPTS = \
	-DLIBC_TYPE=uclibc \
	-DEXAMPLE_DIR=example/luckfox_pico_rtsp_opencv \
	-DEXAMPLE_NAME=luckfox_pico_rtsp_opencv \
	-DOpenCV_DIR=$(STAGING_DIR)/usr/lib/cmake/opencv4 \
	-DWIDTH=$(BR2_PACKAGE_LUCKFOX_PICO_RTSP_OPENCV_WIDTH) \
	-DHEIGHT=$(BR2_PACKAGE_LUCKFOX_PICO_RTSP_OPENCV_HEIGHT)

define LUCKFOX_PICO_RTSP_OPENCV_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/luckfox_pico_rtsp_opencv $(TARGET_DIR)/usr/bin/luckfox_pico_rtsp_opencv
endef

$(eval $(cmake-package))
