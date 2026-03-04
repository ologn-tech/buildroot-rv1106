#!/bin/sh

$HOST_DIR/bin/mk-update_pack.sh -id RV1106 -i $BINARIES_DIR

mv -f $BINARIES_DIR/rootfs.ubi $BINARIES_DIR/rootfs.img 2>/dev/null
rm -f $BINARIES_DIR/rootfs.ubifs

mv -f $BINARIES_DIR/uboot-env.bin $BINARIES_DIR/env.img 2>/dev/null
rm -f $BINARIES_DIR/*.dtb
