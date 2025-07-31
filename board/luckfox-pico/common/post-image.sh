#!/bin/sh

if grep -q "BR2_TARGET_ROOTFS_UBI=y" $BR2_CONFIG; then
    mv -f $BINARIES_DIR/rootfs.ubi $BINARIES_DIR/rootfs.img 2>/dev/null
    rm -f $BINARIES_DIR/rootfs.ubifs
elif grep -q "BR2_TARGET_ROOTFS_EXT2=y" $BR2_CONFIG; then
    mv -f $BINARIES_DIR/rootfs.ext2 $BINARIES_DIR/rootfs.img 2>/dev/null
    rm -f $BINARIES_DIR/rootfs.ext4
fi

mv -f $BINARIES_DIR/uboot-env.bin $BINARIES_DIR/env.img 2>/dev/null
rm -f $BINARIES_DIR/*.dtb
