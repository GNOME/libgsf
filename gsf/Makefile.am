INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)

lib_LTLIBRARIES = libgsf-1.la

if WITH_LIBXML
libxml_sources     = gsf-libxml.c
libxml_headers     = gsf-libxml.h
endif
if WITH_GNOME_VFS
gnomevfs_sources   = gsf-input-gnomevfs.c
gnomevfs_headers   = gsf-input-gnomevfs.h
endif

libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_1_la_SOURCES =		\
	$(libxml_sources)	\
	$(gnomevfs_sources)	\
	gsf-utils.c		\
	gsf-impl-utils.h	\
	\
	gsf-input.c		\
	gsf-input-impl.h	\
	gsf-input-memory.c 	\
	gsf-input-stdio.c 	\
	gsf-input-textline.c 	\
	\
	gsf-infile.c 		\
	gsf-infile-impl.h 	\
	gsf-infile-msole.c 	\
	gsf-infile-zip.c	\
	\
	gsf-output.c 		\
	gsf-output-impl.h	\
	\
	gsf-shared-memory.c	\
	gsf-shared-memory.h

#	gsf-output-memory.c 	\
#	gsf-output-stdio.c


libgsf_1_includedir = $(includedir)/libgsf-1/gsf
libgsf_1_include_HEADERS = 	\
	$(libxml_headers)	\
	$(gnomevfs_headers)	\
	gsf.h			\
	gsf-utils.h		\
	gsf-input.h		\
	gsf-input-memory.h 	\
	gsf-input-stdio.h 	\
	gsf-input-textline.h 	\
	gsf-infile.h 		\
	gsf-infile-msole.h 	\
	gsf-infile-zip.h	\
	gsf-output.h
#gsf-output-memory.h
#gsf-output-stdio.h

noinst_PROGRAMS = test-msole1 test-msole2 test-zip1 test-input1
test_msole1_LDADD = libgsf-1.la $(LIBGSF_LIBS)
test_msole2_LDADD = libgsf-1.la $(LIBGSF_LIBS)
test_zip1_LDADD   = libgsf-1.la $(LIBGSF_LIBS)
test_input1_LDADD = libgsf-1.la $(LIBGSF_LIBS)

# include these just in case we are building a tarball without lixml
# or gnomevfs support.
EXTRA_DIST =	gsf-libxml.c		gsf-libxml.h \
		gsf-input-gnomevfs.c	gsf-input-gnomevfs.h
