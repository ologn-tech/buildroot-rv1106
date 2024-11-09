#!/bin/sh

BOARD_DIR=$(dirname "$0")

install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/kmpp/release_kmpp_rv1106_arm/mpp_vcodec.ko \
        $TARGET_DIR/lib/modules/5.10.160/extra/mpp_vcodec.ko
install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/rockit/release_rockit-ko_rv1106_arm/rockit.ko \
        $TARGET_DIR/lib/modules/5.10.160/extra/rockit.ko
install -D -m 0644 dl/rv1106-ipc-sdk/git/sysdrv/drv_ko/rockit/release_rockit-ko_rv1106_arm/hpmcu_wrap.bin \
        $TARGET_DIR/lib/firmware/hpmcu_wrap.bin

install -m 0755 $BOARD_DIR/S20loadmodules $TARGET_DIR/etc/init.d/S20loadmodules
install -m 0755 $BOARD_DIR/S49usbgadget $TARGET_DIR/etc/init.d/S49usbgadget
