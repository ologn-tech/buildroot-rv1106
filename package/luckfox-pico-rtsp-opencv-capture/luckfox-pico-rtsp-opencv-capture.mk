################################################################################
#
# luckfox-pico-rtsp-opencv-capture
#
################################################################################

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_SITE = https://github.com/LuckfoxTECH/luckfox_pico_rkmpi_example
LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_SITE_METHOD = git
LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_VERSION = 55178250c05542b156ac94c8c08cecef46589abf

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_DEPENDENCIES = opencv4

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_CONF_OPTS = \
	-DLIBC_TYPE=uclibc \
	-DEXAMPLE_DIR=example/luckfox_pico_rtsp_opencv_capture \
	-DEXAMPLE_NAME=luckfox_pico_rtsp_opencv_capture \
	-DOpenCV_DIR=$(STAGING_DIR)/usr/lib/cmake/opencv4

define LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/luckfox_pico_rtsp_opencv_capture $(TARGET_DIR)/usr/bin/luckfox_pico_rtsp_opencv_capture
endef

$(eval $(cmake-package))
