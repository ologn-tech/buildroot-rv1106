################################################################################
#
# luckfox-pico-rtsp-opencv-capture
#
################################################################################

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_SITE = https://github.com/ologn-tech/luckfox_pico_rkmpi_example
LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_SITE_METHOD = git
LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_VERSION = a4b37b350368cdf4787824deb8c2d74b51087367

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_DEPENDENCIES = opencv4

LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_CONF_OPTS = \
	-DLIBC_TYPE=uclibc \
	-DEXAMPLE_DIR=example/luckfox_pico_rtsp_opencv_capture \
	-DEXAMPLE_NAME=luckfox_pico_rtsp_opencv_capture \
	-DOpenCV_DIR=$(STAGING_DIR)/usr/lib/cmake/opencv4 \
	-DWIDTH=$(BR2_PACKAGE_LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_WIDTH) \
	-DHEIGHT=$(BR2_PACKAGE_LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_HEIGHT)

define LUCKFOX_PICO_RTSP_OPENCV_CAPTURE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/luckfox_pico_rtsp_opencv_capture $(TARGET_DIR)/usr/bin/luckfox_pico_rtsp_opencv_capture
endef

$(eval $(cmake-package))
