INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS) $(BONOBO_CFLAGS)

lib_LTLIBRARIES = libgsf-1.la

if WITH_LIBXML
libxml_sources     = gsf-libxml.c
libxml_headers     = gsf-libxml.h
endif
if WITH_GNOME_VFS
gnomevfs_sources   = gsf-input-gnomevfs.c
gnomevfs_headers   = gsf-input-gnomevfs.h
endif
if WITH_BONOBO
bonobo_sources   = gsf-input-bonobo.c gsf-shared-bonobo-stream.c
bonobo_headers   = gsf-input-bonobo.h gsf-shared-bonobo-stream.h
endif

libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_1_la_SOURCES =		\
	$(libxml_sources)	\
	$(gnomevfs_sources)	\
	$(bonobo_sources)	\
	gsf-utils.c		\
	gsf-impl-utils.h	\
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
	$(libxml_headers)	\
	$(gnomevfs_headers)	\
	$(bonobo_headers)	\
	gsf.h			\
	gsf-utils.h		\
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
#gsf-output-memory.h

# include these just in case we are building a tarball without lixml,
# gnomevfs or bonobo support.
EXTRA_DIST =	gsf-libxml.c		gsf-libxml.h 		\
		gsf-input-gnomevfs.c	gsf-input-gnomevfs.h    \
		gsf-input-bonobo.c	gsf-input-bonobo.h      \
		gsf-shared-bonobo-stream.c			\
		gsf-shared-bonobo-stream.h
