INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS)

lib_LTLIBRARIES = libgsf-1.la

libgsf_1_la_LIBADD  = $(LIBGSF_LIBS)
libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_1_la_SOURCES =		\
	gsf-utils.c		\
	gsf-libxml.c		\
	gsf-msole-impl.h	\
	gsf-msole-metadata.c	\
	gsf-timestamp.c		\
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
	gsf-libxml.h		\
	gsf-doc-meta-data.h	\
	gsf-impl-utils.h	\
	gsf-msole-metadata.h	\
	gsf-timestamp.h		\
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
