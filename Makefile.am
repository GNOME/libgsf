AUTOMAKE_OPTIONS = gnu 1.4

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

if WITH_LIBGSF_GNOME
LIBGSF_GNOME_PC = libgsf-gnome-1.pc
GSF_GNOME_DIR   = gsf-gnome
else
GSF_GNOME_DIR   =
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc $(LIBGSF_GNOME_PC)

if WITH_PYTHON
PYTHON_DIR = python
else
PYTHON_DIR = 
endif

SUBDIRS = gsf $(GSF_GNOME_DIR) doc tests $(PYTHON_DIR)

#small hack to get distcheck to work
clean-local:
	rm -f intl/po2tbl.sed

EXTRA_DIST = 			\
	autogen.sh 		\
	gtk-doc.make		\
	libgsf-1.pc.in		\
	libgsf-1.spec		\
	libgsf-1.spec.in	\
	libgsf-gnome-1.pc.in	\
	libgsf-gnome-1.spec	\
	libgsf-gnome-1.spec.in	\
	libgsf-zip.in		\
	README TODO HACKING NEWS BUGS ChangeLog COPYING.LIB
