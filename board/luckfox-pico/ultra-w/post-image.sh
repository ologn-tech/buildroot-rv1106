#!/bin/sh

mv -f $BINARIES_DIR/uboot-env.bin $BINARIES_DIR/env.img 2>/dev/null
rm -f $BINARIES_DIR/*.dtb

echo "Running post-image.sh for luckfox-pico-ultra-w"
# check if the board is luckfox-pico-ultra-w
if grep -q "ultra-w" board/luckfox-pico/ultra-w/uboot-env.txt; then
    echo "Generating rootfs.img"
    mkdir -p $BASE_DIR/rootfs_rv1106
    tar xf $BINARIES_DIR/rootfs.tar -C $BASE_DIR/rootfs_rv1106
    sh board/luckfox-pico/ultra-w/mkfs_ext4.sh $BASE_DIR/rootfs_rv1106 $BINARIES_DIR/rootfs.img 6442450944
    rm -f $BINARIES_DIR/rootfs.tar
    echo "rootfs.img generated successfully"
else
    echo "Board is not luckfox-pico-ultra-w, using post-image.sh"
fi
