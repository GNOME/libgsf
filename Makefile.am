DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

if WITH_LIBGSF_GNOME
LIBGSF_GNOME_PC = libgsf-gnome-1.pc
endif

if PLATFORM_WIN32
LIBGSF_WIN32_PC = libgsf-win32-1.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc $(LIBGSF_GNOME_PC) $(LIBGSF_WIN32_PC)

if WITH_PYTHON
PYTHON_DIR = python
else
PYTHON_DIR = 
endif

SUBDIRS = gsf gsf-gnome gsf-win32 doc tests $(PYTHON_DIR)

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
	libgsf-win32-1.pc.in	\
	libgsf-zip.in		\
	README TODO HACKING NEWS BUGS ChangeLog COPYING.LIB
