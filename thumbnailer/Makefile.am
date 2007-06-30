@INTLTOOL_SCHEMAS_RULE@

AM_CPPFLAGS = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)
LDADD = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

bin_PROGRAMS = gsf-office-thumbnailer

gsf_office_thumbnailer_SOURCES =	\
	main.c

schema_files = \
	gsf-office-thumbnailer.schemas.in

schemas = gsf-office-thumbnailer.schemas
schemadir = $(GCONF_SCHEMA_FILE_DIR)
if GCONF_SCHEMAS_INSTALL
schema_DATA = $(schemas)
endif

install-data-local:
if GCONF_SCHEMAS_INSTALL
if WITH_WIN32
if !CROSS_COMPILING
	if test -z "$(DESTDIR)" -a -n "$(GCONF_SCHEMA_FILE_DIR)" ; then \
	  for p in $(schemas); do \
	    (echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE); \
	     echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat; \
	    cmd /c _temp.bat; \
	  done \
	fi
endif
else
	if test -z "$(DESTDIR)" -a -n "$(GCONF_SCHEMA_FILE_DIR)" ; then \
	  for p in $(schemas); do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	      $(GCONFTOOL) --makefile-install-rule $$p; \
	  done \
	fi
endif
endif

EXTRA_DIST =         	\
	$(schema_files)  

DISTCLEANFILES = \
	$(schema_DATA)
