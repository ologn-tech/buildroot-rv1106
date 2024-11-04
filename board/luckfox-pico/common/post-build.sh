#!/bin/sh

BOARD_DIR=$(dirname "$0")

install -m 0755 $BOARD_DIR/S20loadmodules $TARGET_DIR/etc/init.d/
install -m 0755 $BOARD_DIR/S49usbgadget $TARGET_DIR/etc/init.d/
