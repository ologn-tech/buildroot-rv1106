#!/bin/sh

BOARD_DIR=$(dirname "$0")

install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/rockit/release_rockit-ko_rv1106_arm/hpmcu_wrap.bin \
        $TARGET_DIR/lib/firmware/hpmcu_wrap.bin
install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/kmpp/release_kmpp_rv1106_arm/mpp_vcodec.ko \
        $TARGET_DIR/lib/modules/5.10.160/extra/mpp_vcodec.ko
install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/rockit/release_rockit-ko_rv1106_arm/rockit.ko \
        $TARGET_DIR/lib/modules/5.10.160/extra/rockit.ko

install -D -m 0644 dl/rv1106-ipc-sdk/git/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf/isp_iqfiles/sc3336_CMK-OT2119-PC1_30IRC-F16.json \
        $TARGET_DIR/etc/iqfiles/sc3336_CMK-OT2119-PC1_30IRC-F16.json
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib/librkaiq.so \
        $TARGET_DIR/usr/lib/librkaiq.so
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/iva/iva/librockiva/rockiva-rv1106-Linux/lib/librknnmrt.so \
        $TARGET_DIR/usr/lib/librknnmrt.so
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/iva/iva/librockiva/rockiva-rv1106-Linux/lib/librockiva.so \
        $TARGET_DIR/usr/lib/librockiva.so
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib/librockchip_mpp.so \
        $TARGET_DIR/usr/lib/librockchip_mpp.so
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib/librockchip_mpp.so.0 \
        $TARGET_DIR/usr/lib/librockchip_mpp.so.0
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib/librockchip_mpp.so.1 \
        $TARGET_DIR/usr/lib/librockchip_mpp.so.1
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib/librga.so \
        $TARGET_DIR/usr/lib/librga.so
install -D -m 0644 dl/rv1106-ipc-sdk/git/media/rockit/rockit/lib/lib32/librockit.so \
        $TARGET_DIR/usr/lib/librockit.so

install -m 0755 $BOARD_DIR/S20loadmodules $TARGET_DIR/etc/init.d/S20loadmodules
install -m 0755 $BOARD_DIR/S49usbgadget $TARGET_DIR/etc/init.d/S49usbgadget

install -m 0644 $BOARD_DIR/dhcpd.conf $TARGET_DIR/etc/dhcp/dhcpd.conf
install -m 0644 $BOARD_DIR/fw_env.config $TARGET_DIR/etc/fw_env.config
