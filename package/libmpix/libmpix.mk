################################################################################
#
# libmpix
#
################################################################################

LIBMPIX_VERSION = 98a8fcc0918166aa489377c5ce06d3199de17a24
LIBMPIX_SITE = https://github.com/libmpix/libmpix
LIBMPIX_SITE_METHOD = git
LIBMPIX_LICENSE = Apache-2.0
LIBMPIX_LICENSE_FILES = LICENSE

LIBMPIX_INSTALL_TARGET = NO

$(eval $(generic-package))
