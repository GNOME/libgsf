ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc

if WITH_WIN32
  pkgconfig_DATA += libgsf-win32-1.pc
endif

SUBDIRS = po gsf gsf-win32 doc tools tests thumbnailer python

EXTRA_DIST = BUGS HACKING	\
	msvc/dumpdef.pl

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --enable-gtk-doc \
	--enable-schemas-install \
	--enable-introspection=auto \
	--with-gir-dir=\$${datadir}/gir-1.0 \
	--with-typelib-dir=\$${libdir}/girepository-1.0

