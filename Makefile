##############################################
# OpenWrt Makefile for UltiFi 3D printing package
##############################################
include $(TOPDIR)/rules.mk

PKG_NAME := ultifi
PKG_VERSION := 0.1.0
PKG_RELEASE := 5

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/ultifi
	SECTION:=mods
	CATEGORY:=Doodle3D
	MENU:=0
#	DEFAULT:=y
	TITLE:=UltiFi 3D printing manager
	URL:=http://www.doodle3d.com/wifibox
	DEPENDS:=
endef

define Package/ultifi/description
	UltiFi 3D printing manager
endef

define Package/ultifi/config
	source "$(SOURCE)/Config.in"
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
#	no configuration necessary
endef

define Build/Compile directives
#	no compilation necessary
endef

ULTIFI_BASE_DIR := $(PKG_BUILD_DIR)
PRINTERMANAGER_BASE_DIR := $(PKG_BUILD_DIR)/printermanager
TGT_DIR_SUFFIX := UltiFi

define Package/ultifi/install
	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)
	$(INSTALL_DIR) $(1)/etc/init.d
	
	$(INSTALL_BIN) $(ULTIFI_BASE_DIR)/script/ultifi_init $(1)/etc/init.d/ultifi
	$(INSTALL_BIN) $(ULTIFI_BASE_DIR)/script/PrinterManager.sh $(1)/$(TGT_DIR_SUFFIX)
	$(INSTALL_BIN) $(ULTIFI_BASE_DIR)/script/RunPrinterInterface.sh $(1)/$(TGT_DIR_SUFFIX)
	$(INSTALL_BIN) $(PRINTERMANAGER_BASE_DIR)/ManageConnection.bin $(1)/$(TGT_DIR_SUFFIX)

ifeq ($(CONFIG_ULTIFI_WEB_INTERFACE),y)
#	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www
#	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www/UltiFi-static
	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www/UltiFi-static/images
#	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www/cgi-bin
	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www/cgi-bin/UltiFi
	$(INSTALL_DIR) $(1)/$(TGT_DIR_SUFFIX)/www/template
	$(INSTALL_DIR) $(1)/www/cgi-bin
	
	$(CP) $(ULTIFI_BASE_DIR)/www/* $(1)/$(TGT_DIR_SUFFIX)/www/
	$(LN) -s /$(TGT_DIR_SUFFIX)/www/index.html $(1)/www/ultifi.html
	$(LN) -s /$(TGT_DIR_SUFFIX)/www/UltiFi-static $(1)/www/
	$(LN) -s /$(TGT_DIR_SUFFIX)/www/cgi-bin/UltiFi $(1)/www/cgi-bin/
endif
endef

define Package/ultifi/postinst
if [ -z "$IPKG_INSTROOT" ]; then
	/etc/init.d/ultifi enable
fi
endef

define Package/ultifi/prerm
if [ -z "$IPKG_INSTROOT" ]; then
	/etc/init.d/ultifi disable
fi
endef

$(eval $(call BuildPackage,ultifi))
