if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32

install-libtool-import-lib:
	$(INSTALL) .libs/libgsf-1.dll.a $(DESTDIR)$(libdir)

uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libgsf-1.dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS)

lib_LTLIBRARIES = libgsf-1.la

libgsf_1_la_LIBADD  = $(LIBGSF_LIBS)
libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO) $(no_undefined)
libgsf_1_la_SOURCES =		\
	gsf-utils.c		\
	gsf-libxml.c		\
	gsf-msole-impl.h	\
	gsf-msole-utils.c	\
	gsf-timestamp.c		\
	gsf-zip-impl.h		\
	gsf-zip-utils.c		\
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
	gsf-output-iochannel.c	\
	gsf-output-memory.c	\
	gsf-output-stdio.c	\
	gsf-output-gzip.c 	\
	gsf-output-transaction.c	\
	\
	gsf-outfile.c 		\
	gsf-outfile-impl.h 	\
	gsf-outfile-msole.c 	\
	gsf-outfile-zip.c	\
	\
	gsf-shared-memory.c	\
	gsf-shared-memory.h	\
	gsf-structured-blob.c
#	gsf-command-context.c	\
#	gsf-io-context.c

libgsf_1_includedir = $(includedir)/libgsf-1/gsf
libgsf_1_include_HEADERS = 	\
	gsf.h			\
	gsf-utils.h		\
	gsf-libxml.h		\
	gsf-doc-meta-data.h	\
	gsf-impl-utils.h	\
	gsf-msole-utils.h	\
	gsf-timestamp.h		\
	\
	gsf-input.h		\
	gsf-input-memory.h 	\
	gsf-input-stdio.h 	\
	gsf-input-textline.h 	\
	gsf-input-gzip.h 	\
	\
	gsf-infile.h 		\
	gsf-infile-msole.h 	\
	gsf-infile-msvba.h 	\
	gsf-infile-zip.h	\
	\
	gsf-output.h		\
	gsf-output-iochannel.h	\
	gsf-output-memory.h	\
	gsf-output-stdio.h	\
	gsf-output-gzip.h 	\
	\
	gsf-outfile.h 		\
	gsf-outfile-msole.h	\
	gsf-outfile-zip.h       \
	\
	gsf-structured-blob.h
#	gsf-command-context.h	\
#	gsf-io-context.h

install-data-local: install-libtool-import-lib

uninstall-local: uninstall-libtool-import-lib
