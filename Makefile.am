pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc

if WITH_LIBGSF_GNOME
  pkgconfig_DATA += libgsf-gnome-1.pc
endif
if PLATFORM_WIN32
  pkgconfig_DATA += libgsf-win32-1.pc
endif

SUBDIRS = po gsf gsf-gnome thumbnailer gsf-win32 doc tests

if WITH_PYTHON
  SUBDIRS += python
endif

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

#small hack to get distcheck to work
clean-local:
	rm -f intl/po2tbl.sed

EXTRA_DIST = config.rpath m4/ChangeLog  			\
	libgsf-1.spec		\
	libgsf-gnome-1.spec	\
	README TODO HACKING NEWS BUGS ChangeLog COPYING.LIB

ACLOCAL_AMFLAGS = -I m4
