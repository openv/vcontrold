# -*-makefile-*-
#
# Copyright (C) 2012 by Johannes Fischer <wir@konrad8.de>
#
# See CREDITS for details about who has contributed to this project.
#
# For further information about the PTXdist project and license conditions
# see the README file.
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_VCONTROLD) += vcontrold

#
# Paths and names
#
ifdef PTXCONF_VCONTROLD_TRUNK
VCONTROLD_VERSION	:= trunk
else
VCONTROLD_VERSION	:= 20121020
VCONTROLD_MD5		:=
endif
VCONTROLD		:= vcontrold-$(VCONTROLD_VERSION)
VCONTROLD_URL		:= file://$(PTXDIST_WORKSPACE)/local_src/$(VCONTROLD)
VCONTROLD_DIR		:= $(BUILDDIR)/$(VCONTROLD)
VCONTROLD_BUILD_OOT	:= YES
VCONTROLD_LICENSE	:= unknown

# ----------------------------------------------------------------------------
# Extract
# ----------------------------------------------------------------------------

ifdef PTXCONF_VCONTROLD_TRUNK
$(STATEDIR)/vcontrold.extract: $(STATEDIR)/autogen-tools
endif

$(STATEDIR)/vcontrold.extract:
	@$(call targetinfo)
	@$(call clean, $(VCONTROLD_DIR))
	@$(call extract, VCONTROLD)
ifdef PTXCONF_VCONTROLD_TRUNK
	cd $(VCONTROLD_DIR) && sh autogen.sh
else
	cd $(VCONTROLD_DIR) && [ -f configure ] || sh autogen.sh
endif
	@$(call patchin, VCONTROLD)
	@$(call touch)

# ----------------------------------------------------------------------------
# Prepare
# ----------------------------------------------------------------------------

#VCONTROLD_CONF_ENV	:= $(CROSS_ENV)

#
# autoconf
#
VCONTROLD_CONF_TOOL	:= autoconf
VCONTROLD_CONF_OPT	:= $(CROSS_AUTOCONF_USR) --with-xml2-include-dir=$(BUILDDIR)/libxml2-2.7.7/include

  

#$(STATEDIR)/vcontrold.prepare:
#	@$(call targetinfo)
#	@$(call world/prepare, VCONTROLD)
#	@$(call touch)

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

#$(STATEDIR)/vcontrold.compile:
#	@$(call targetinfo)
#	@$(call world/compile, VCONTROLD)
#	@$(call touch)

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------

#$(STATEDIR)/vcontrold.install:
#	@$(call targetinfo)
#	@$(call world/install, VCONTROLD)
#	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/vcontrold.targetinstall:
	@$(call targetinfo)

	@$(call install_init, vcontrold)
	@$(call install_fixup, vcontrold, PRIORITY, optional)
	@$(call install_fixup, vcontrold, SECTION, base)
	@$(call install_fixup, vcontrold, AUTHOR, "Johannes Fischer <wir@konrad8.de>")
	@$(call install_fixup, vcontrold, DESCRIPTION, missing)

#	#
#	# example code:; copy all libraries, links and binaries
#	#

	@for i in $(shell cd $(VCONTROLD_PKGDIR) && find bin sbin usr/bin usr/sbin -type f); do \
		$(call install_copy, vcontrold, 0, 0, 0755, -, /$$i); \
	done
	@for i in $(shell cd $(VCONTROLD_PKGDIR) && find lib usr/lib -name "*.so*"); do \
		$(call install_copy, vcontrold, 0, 0, 0644, -, /$$i); \
	done
	@links="$(shell cd $(VCONTROLD_PKGDIR) && find lib usr/lib -type l)"; \
	if [ -n "$$links" ]; then \
		for i in $$links; do \
			from="`readlink $(VCONTROLD_PKGDIR)/$$i`"; \
			to="/$$i"; \
			$(call install_link, vcontrold, $$from, $$to); \
		done; \
	fi

#	#
#	# FIXME: add all necessary things here
#	#

	@$(call install_finish, vcontrold)

	@$(call touch)

# ----------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------

#$(STATEDIR)/vcontrold.clean:
#	@$(call targetinfo)
#	@$(call clean_pkg, VCONTROLD)

# vim: syntax=make
