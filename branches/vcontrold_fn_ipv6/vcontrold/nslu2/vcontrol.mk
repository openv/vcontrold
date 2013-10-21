###########################################################
#
# vcontrol
#
###########################################################
#
# VCONTROL_VERSION, VCONTROL_SITE and VCONTROL_SOURCE define
# the upstream location of the source code for the package.
# VCONTROL_DIR is the directory which is created when the source
# archive is unpacked.
# VCONTROL_UNZIP is the command used to unzip the source.
# It is usually "zcat" (for .gz) or "bzcat" (for .bz2)
#
# You should change all these variables to suit your package.
# Please make sure that you add a description, and that you
# list all your packages' dependencies, seperated by commas.
# 
# If you list yourself as MAINTAINER, please give a valid email
# address, and indicate your irc nick if it cannot be easily deduced
# from your name or email address.  If you leave MAINTAINER set to
# "NSLU2 Linux" other developers will feel free to edit.
#
VCONTROL_SITE=http://www.kr.home
VCONTROL_VERSION=1.0
VCONTROL_SOURCE=vcontrol-$(VCONTROL_VERSION).tar.gz
VCONTROL_DIR=vcontrol-$(VCONTROL_VERSION)
VCONTROL_UNZIP=zcat
VCONTROL_MAINTAINER=mschroether@gmx.de
VCONTROL_DESCRIPTION=Viessman Vitotronic SW interface
VCONTROL_SECTION=extras
VCONTROL_PRIORITY=optional
VCONTROL_DEPENDS=libxml2
VCONTROL_SUGGESTS=
VCONTROL_CONFLICTS=

#
# VCONTROL_IPK_VERSION should be incremented when the ipk changes.
#
VCONTROL_IPK_VERSION=5

#
# VCONTROL_CONFFILES should be a list of user-editable files
VCONTROL_CONFFILES=/opt/etc/vcontrold/vcontrold.xml /opt/etc/vcontrold/vito.xml /opt/etc/init.d/S99vcontrol

#
# VCONTROL_PATCHES should list any patches, in the the order in
# which they should be applied to the source code.
#
#VCONTROL_PATCHES=$(VCONTROL_SOURCE_DIR)/configure.patch

#
# If the compilation of the package requires additional
# compilation or linking flags, then list them here.
#
# VCONTROL_CPPFLAGS=-g
# VCONTROL_LDFLAGS=-lxml2

#
# VCONTROL_BUILD_DIR is the directory in which the build is done.
# VCONTROL_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
# VCONTROL_IPK_DIR is the directory in which the ipk is built.
# VCONTROL_IPK is the name of the resulting ipk files.
#
# You should not change any of these variables.
#
VCONTROL_BUILD_DIR=$(BUILD_DIR)/vcontrol
VCONTROL_SOURCE_DIR=$(SOURCE_DIR)/vcontrol
VCONTROL_IPK_DIR=$(BUILD_DIR)/vcontrol-$(VCONTROL_VERSION)-ipk
VCONTROL_IPK=$(BUILD_DIR)/vcontrol_$(VCONTROL_VERSION)-$(VCONTROL_IPK_VERSION)_$(TARGET_ARCH).ipk

.PHONY: vcontrol-source vcontrol-unpack vcontrol vcontrol-stage vcontrol-ipk vcontrol-clean vcontrol-dirclean vcontrol-check

#
# This is the dependency on the source code.  If the source is missing,
# then it will be fetched from the site using wget.
#
$(DL_DIR)/$(VCONTROL_SOURCE):
	$(WGET) -P $(@D) $(VCONTROL_SITE)/$(@F) || \
	$(WGET) -P $(@D) $(SOURCES_NLO_SITE)/$(@F)

#
# The source code depends on it existing within the download directory.
# This target will be called by the top level Makefile to download the
# source code's archive (.tar.gz, .bz2, etc.)
#
vcontrol-source: $(DL_DIR)/$(VCONTROL_SOURCE) $(VCONTROL_PATCHES)

#
# This target unpacks the source code in the build directory.
# If the source archive is not .tar.gz or .tar.bz2, then you will need
# to change the commands here.  Patches to the source code are also
# applied in this target as required.
#
# This target also configures the build within the build directory.
# Flags such as LDFLAGS and CPPFLAGS should be passed into configure
# and NOT $(MAKE) below.  Passing it to configure causes configure to
# correctly BUILD the Makefile with the right paths, where passing it
# to Make causes it to override the default search paths of the compiler.
#
# If the compilation of the package requires other packages to be staged
# first, then do that first (e.g. "$(MAKE) <bar>-stage <baz>-stage").
#
# If the package uses  GNU libtool, you should invoke $(PATCH_LIBTOOL) as
# shown below to make various patches to it.
#
$(VCONTROL_BUILD_DIR)/.configured: $(DL_DIR)/$(VCONTROL_SOURCE) $(VCONTROL_PATCHES) make/vcontrol.mk
	$(MAKE) libxml2-stage
	rm -rf $(BUILD_DIR)/$(VCONTROL_DIR) $(@D)
	$(VCONTROL_UNZIP) $(DL_DIR)/$(VCONTROL_SOURCE) | tar -C $(BUILD_DIR) -xvf -
	if test -n "$(VCONTROL_PATCHES)" ; \
		then cat $(VCONTROL_PATCHES) | \
		patch -d $(BUILD_DIR)/$(VCONTROL_DIR) -p0 ; \
	fi
	if test "$(BUILD_DIR)/$(VCONTROL_DIR)" != "$(@D)" ; \
		then mv $(BUILD_DIR)/$(VCONTROL_DIR) $(@D) ; \
	fi
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		CPPFLAGS="$(STAGING_CPPFLAGS) $(VCONTROL_CPPFLAGS)" \
		LDFLAGS="$(STAGING_LDFLAGS) $(VCONTROL_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/opt \
		--with-xml2-include-dir=$(STAGING_PREFIX)/include/libxml2 \
		--with-xml2-lib-dir=$(STAGING_PREFIX)/lib \
		--disable-nls \
		--disable-static \
	)
#	$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

vcontrol-unpack: $(VCONTROL_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(VCONTROL_BUILD_DIR)/.built: $(VCONTROL_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
vcontrol: $(VCONTROL_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(VCONTROL_BUILD_DIR)/.staged: $(VCONTROL_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

vcontrol-stage: $(VCONTROL_BUILD_DIR)/.staged

#
# This rule creates a control file for ipkg.  It is no longer
# necessary to create a seperate control file under sources/vcontrol
#
$(VCONTROL_IPK_DIR)/CONTROL/control:
	@install -d $(@D)
	@rm -f $@
	@echo "Package: vcontrol" >>$@
	@echo "Architecture: $(TARGET_ARCH)" >>$@
	@echo "Priority: $(VCONTROL_PRIORITY)" >>$@
	@echo "Section: $(VCONTROL_SECTION)" >>$@
	@echo "Version: $(VCONTROL_VERSION)-$(VCONTROL_IPK_VERSION)" >>$@
	@echo "Maintainer: $(VCONTROL_MAINTAINER)" >>$@
	@echo "Source: $(VCONTROL_SITE)/$(VCONTROL_SOURCE)" >>$@
	@echo "Description: $(VCONTROL_DESCRIPTION)" >>$@
	@echo "Depends: $(VCONTROL_DEPENDS)" >>$@
	@echo "Suggests: $(VCONTROL_SUGGESTS)" >>$@
	@echo "Conflicts: $(VCONTROL_CONFLICTS)" >>$@

#
# This builds the IPK file.
#
# Binaries should be installed into $(VCONTROL_IPK_DIR)/opt/sbin or $(VCONTROL_IPK_DIR)/opt/bin
# (use the location in a well-known Linux distro as a guide for choosing sbin or bin).
# Libraries and include files should be installed into $(VCONTROL_IPK_DIR)/opt/{lib,include}
# Configuration files should be installed in $(VCONTROL_IPK_DIR)/opt/etc/vcontrol/...
# Documentation files should be installed in $(VCONTROL_IPK_DIR)/opt/doc/vcontrol/...
# Daemon startup scripts should be installed in $(VCONTROL_IPK_DIR)/opt/etc/init.d/S??vcontrol
#
# You may need to patch your application to make it use these locations.
#
$(VCONTROL_IPK): $(VCONTROL_BUILD_DIR)/.built
	rm -rf $(VCONTROL_IPK_DIR) $(BUILD_DIR)/vcontrol_*_$(TARGET_ARCH).ipk
	$(MAKE) -C $(VCONTROL_BUILD_DIR) DESTDIR=$(VCONTROL_IPK_DIR) install-strip
	install -d $(VCONTROL_IPK_DIR)/opt/etc/vcontrold
	install -m 644 $(VCONTROL_SOURCE_DIR)/vcontrold.xml $(VCONTROL_IPK_DIR)/opt/etc/vcontrold/vcontrold.xml
	install -m 644 $(VCONTROL_SOURCE_DIR)/vito.xml $(VCONTROL_IPK_DIR)/opt/etc/vcontrold/vito.xml
	install -d $(VCONTROL_IPK_DIR)/opt/etc/init.d
	install -m 755 $(VCONTROL_SOURCE_DIR)/rc.vcontrol $(VCONTROL_IPK_DIR)/opt/etc/init.d/S99vcontrol
#	sed -i -e '/^#!/aOPTWARE_TARGET=${OPTWARE_TARGET}' $(VCONTROL_IPK_DIR)/opt/etc/init.d/SXXvcontrol
	$(MAKE) $(VCONTROL_IPK_DIR)/CONTROL/control
#	install -m 755 $(VCONTROL_SOURCE_DIR)/postinst $(VCONTROL_IPK_DIR)/CONTROL/postinst
#	sed -i -e '/^#!/aOPTWARE_TARGET=${OPTWARE_TARGET}' $(VCONTROL_IPK_DIR)/CONTROL/postinst
#	install -m 755 $(VCONTROL_SOURCE_DIR)/prerm $(VCONTROL_IPK_DIR)/CONTROL/prerm
#	sed -i -e '/^#!/aOPTWARE_TARGET=${OPTWARE_TARGET}' $(VCONTROL_IPK_DIR)/CONTROL/prerm
#	if test -n "$(UPD-ALT_PREFIX)"; then \
		sed -i -e '/^[ 	]*update-alternatives /s|update-alternatives|$(UPD-ALT_PREFIX)/bin/&|' \
			$(VCONTROL_IPK_DIR)/CONTROL/postinst $(VCONTROL_IPK_DIR)/CONTROL/prerm; \
	fi
	echo $(VCONTROL_CONFFILES) | sed -e 's/ /\n/g' > $(VCONTROL_IPK_DIR)/CONTROL/conffiles
	cd $(BUILD_DIR); $(IPKG_BUILD) $(VCONTROL_IPK_DIR)

#
# This is called from the top level makefile to create the IPK file.
#
vcontrol-ipk: $(VCONTROL_IPK)

#
# This is called from the top level makefile to clean all of the built files.
#
vcontrol-clean:
	rm -f $(VCONTROL_BUILD_DIR)/.built
	-$(MAKE) -C $(VCONTROL_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
vcontrol-dirclean:
	rm -rf $(BUILD_DIR)/$(VCONTROL_DIR) $(VCONTROL_BUILD_DIR) $(VCONTROL_IPK_DIR) $(VCONTROL_IPK)
#
#
# Some sanity check for the package.
#
vcontrol-check: $(VCONTROL_IPK)
	perl scripts/optware-check-package.pl --target=$(OPTWARE_TARGET) $^
