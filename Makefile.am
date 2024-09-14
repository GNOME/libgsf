ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc

if WITH_WIN32
  pkgconfig_DATA += libgsf-win32-1.pc
endif

SUBDIRS = po gsf gsf-win32 doc tools tests thumbnailer python

EXTRA_DIST = BUGS HACKING	\
	msvc/dumpdef.pl

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-introspection
