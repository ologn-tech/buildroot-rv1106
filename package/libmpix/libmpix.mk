################################################################################
#
# libmpix
#
################################################################################

LIBMPIX_VERSION = 70e5cc803aea988e16358ae161c65184df1a3050
LIBMPIX_SITE = https://github.com/ologn-tech/libmpix
LIBMPIX_SITE_METHOD = git
LIBMPIX_LICENSE = Apache-2.0
LIBMPIX_LICENSE_FILES = LICENSE

LIBMPIX_INSTALL_TARGET = NO

$(eval $(generic-package))
