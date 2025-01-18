#!/bin/sh

mv -f $BINARIES_DIR/rootfs.ubi $BINARIES_DIR/rootfs.img 2>/dev/null
mv -f $BINARIES_DIR/uboot-env.bin $BINARIES_DIR/env.img 2>/dev/null
rm -f $BINARIES_DIR/*.dtb
rm -f $BINARIES_DIR/rootfs.ubifs
