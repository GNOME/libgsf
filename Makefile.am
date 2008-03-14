ACLOCAL_AMFLAGS = -I m4 $(ACLOCAL_FLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc

if WITH_LIBGSF_GNOME
  pkgconfig_DATA += libgsf-gnome-1.pc
endif
if WITH_WIN32
  pkgconfig_DATA += libgsf-win32-1.pc
endif
if WITH_GIO
  pkgconfig_DATA += libgsf-gvfs-1.pc
endif

SUBDIRS = po gsf gsf-gnome gsf-win32 gsf-gvfs doc tools tests m4

if WITH_PYTHON
  SUBDIRS += python
endif
if WITH_GCONF
  SUBDIRS += thumbnailer
endif

EXTRA_DIST = BUGS HACKING	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	libgsf-1.spec		\
	libgsf-gnome-1.spec	\
	dumpdef.pl

DISTCLEANFILES = intltool-extract intltool-merge intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --enable-gtk-doc
