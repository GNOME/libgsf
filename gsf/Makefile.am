INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS) $(BONOBO_CFLAGS)

if WITH_LIBGSF_GNOME
target_libgsf_gnome = libgsf-gnome-1.la
else
target_libgsf_gnome =
endif

lib_LTLIBRARIES = libgsf-1.la $(target_libgsf_gnome)

libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_1_la_SOURCES =		\
	gsf-utils.c		\
	gsf-libxml.c		\
	gsf-libxml.h		\
	gsf-msole-impl.h	\
	gsf-zip-impl.h		\
	\
	gsf-input.c		\
	gsf-input-impl.h	\
	gsf-input-memory.c 	\
	gsf-input-stdio.c 	\
	gsf-input-textline.c 	\
	gsf-input-gzip.c 	\
	\
	gsf-infile.c 		\
	gsf-infile-impl.h 	\
	gsf-infile-msole.c 	\
	gsf-infile-msvba.c 	\
	gsf-infile-zip.c	\
	\
	gsf-output.c 		\
	gsf-output-impl.h	\
	gsf-output-stdio.c	\
	\
	gsf-outfile.c 		\
	gsf-outfile-impl.h 	\
	gsf-outfile-msole.c 	\
	\
	gsf-shared-memory.c	\
	gsf-shared-memory.h	\
	gsf-io-context.c
#	gsf-output-memory.c

libgsf_1_includedir = $(includedir)/libgsf-1/gsf
libgsf_1_include_HEADERS = 	\
	gsf.h			\
	gsf-utils.h		\
	gsf-impl-utils.h	\
	gsf-input.h		\
	gsf-input-memory.h 	\
	gsf-input-stdio.h 	\
	gsf-input-textline.h 	\
	gsf-input-gzip.h 	\
	gsf-infile.h 		\
	gsf-infile-msole.h 	\
	gsf-infile-msvba.h 	\
	gsf-infile-zip.h	\
	gsf-io-context.h	\
	gsf-output.h		\
	gsf-output-stdio.h	\
	gsf-outfile.h 		\
	gsf-outfile-msole.h
#	gsf-output-memory.h

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
