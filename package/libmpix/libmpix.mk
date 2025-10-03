################################################################################
#
# libmpix
#
################################################################################

LIBMPIX_VERSION = ed6214ad3d2ccf058812d3b3a614b937a7fe582e
LIBMPIX_SITE = https://github.com/libmpix/libmpix
LIBMPIX_SITE_METHOD = git
LIBMPIX_LICENSE = Apache-2.0
LIBMPIX_LICENSE_FILES = LICENSE

LIBMPIX_INSTALL_TARGET = NO

$(eval $(generic-package))
