INCLUDES = -I$(top_srcdir) $(LIBGSF_GNOME_CFLAGS)

if WITH_LIBGSF_GNOME
target_libgsf_gnome = libgsf-gnome-1.la
else
target_libgsf_gnome =
endif

lib_LTLIBRARIES = $(target_libgsf_gnome)

libgsf_gnome_1_la_LIBADD  = $(LIBGSF_GNOME_LIBS)
libgsf_gnome_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_gnome_1_la_SOURCES =	\
	gsf-input-gnomevfs.c 	\
	gsf-output-gnomevfs.c	\
	gsf-input-bonobo.c	\
	gsf-shared-bonobo-stream.c

libgsf_gnome_1_includedir = $(includedir)/libgsf-1/gsf
libgsf_gnome_1_include_HEADERS =  \
	gsf-input-gnomevfs.h	\
	gsf-output-gnomevfs.h	\
	gsf-input-bonobo.h 	\
	gsf-shared-bonobo-stream.h

# include these just in case we are building a tarball without
# gnomevfs or bonobo support.
EXTRA_DIST = $(libgsf_gnome_1_la_SOURCES) $(libgsf_gnome_1_include_HEADERS)
