################################################################################
#
# fluent-bit
#
################################################################################

FLUENT_BIT_VERSION = 2.2.3
FLUENT_BIT_SITE = $(call github,fluent,fluent-bit,v$(FLUENT_BIT_VERSION))
FLUENT_BIT_LICENSE = Apache-2.0
FLUENT_BIT_LICENSE_FILES = LICENSE
FLUENT_BIT_CPE_ID_VENDOR = treasuredata
FLUENT_BIT_CPE_ID_PRODUCT = fluent_bit
FLUENT_BIT_DEPENDENCIES = host-bison host-flex libyaml openssl

FLUENT_BIT_CONF_OPTS += \
	-DFLB_DEBUG=No \
	-DFLB_RELEASE=Yes \
	-DFLB_EXAMPLES=No \
	-DFLB_CHUNK_TRACE=No \
	-DFLB_PREFER_SYSTEM_LIBS=Yes \
	-DFLB_BACKTRACE=No

ifeq ($(BR2_PACKAGE_FLUENT_BIT_WASM),y)
FLUENT_BIT_WAMR_ARCH = $(call qstrip,$(BR2_PACKAGE_FLUENT_BIT_WASM_ARCH))

# https://github.com/bytecodealliance/wasm-micro-runtime/issues/625
# Fix unknown opcode 'ldc1', seen on mips32r2 and mips64r2.
ifeq ($(FLUENT_BIT_WAMR_ARCH),MIPS)
FLUENT_BIT_CONF_OPTS += \
	-DWAMR_BUILD_INVOKE_NATIVE_GENERAL=1
endif

FLUENT_BIT_CONF_OPTS += -DFLB_WASM=Yes \
	-DWAMR_BUILD_TARGET=$(FLUENT_BIT_WAMR_ARCH)
else
FLUENT_BIT_CONF_OPTS += -DFLB_WASM=No
endif

ifeq ($(BR2_PACKAGE_LUAJIT),y)
FLUENT_BIT_CONF_OPTS += -DFLB_LUAJIT=Yes
FLUENT_BIT_DEPENDENCIES += luajit
else
FLUENT_BIT_CONF_OPTS += -DFLB_LUAJIT=No
endif

# Force bundled miniz to be linked statically.
# https://github.com/fluent/fluent-bit/issues/6711
FLUENT_BIT_CONF_OPTS += \
	-DBUILD_SHARED_LIBS=OFF

# Move the config files from /usr/etc/ to /etc/.
# https://github.com/fluent/fluent-bit/issues/6619
FLUENT_BIT_CONF_OPTS += \
	-DCMAKE_INSTALL_SYSCONFDIR="/etc/"

ifeq ($(BR2_PACKAGE_LIBEXECINFO),y)
FLUENT_BIT_DEPENDENCIES += libexecinfo
FLUENT_BIT_LDFLAGS += -lexecinfo
endif

ifeq ($(BR2_TOOLCHAIN_USES_GLIBC),)
FLUENT_BIT_DEPENDENCIES += musl-fts
FLUENT_BIT_LDFLAGS += -lfts
endif

# Uses __atomic_compare_exchange_8
ifeq ($(BR2_TOOLCHAIN_HAS_LIBATOMIC),y)
FLUENT_BIT_LDFLAGS += -latomic
endif

FLUENT_BIT_CONF_OPTS += \
	-DCMAKE_EXE_LINKER_FLAGS="$(FLUENT_BIT_LDFLAGS)"

define FLUENT_BIT_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 package/fluent-bit/S99fluent-bit \
		$(TARGET_DIR)/etc/init.d/S99fluent-bit
endef

$(eval $(cmake-package))
