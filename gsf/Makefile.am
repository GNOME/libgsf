INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS)

lib_LTLIBRARIES = libgsf-1.la

gnomevfs_files = gsf-input-gnomevfs.c gsf-input-gnomevfs.h 

if WITH_GNOME_VFS
gnomevfs_support = $(gnomevfs_files)
else
gnomevfs_support =
endif

libgsf_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_1_la_SOURCES =		\
	gsf-utils.c		\
	gsf-impl-utils.h	\
	\
	gsf-input.c		\
	gsf-input-impl.h	\
	gsf-input-memory.c 	\
	gsf-input-stdio.c 	\
	\
	gsf-infile.c 		\
	gsf-infile-impl.h 	\
	gsf-infile-msole.c 	\
	gsf-infile-zip.c	\
	\
	gsf-output.c 		\
	gsf-output-impl.h 
	gsf-output-memory.c 	\
	gsf-output-stdio.c 	\
	\
	$(gnomevfs_support)

libgsf_1_includedir = $(includedir)/libgsf-1/gsf
libgsf_1_include_HEADERS = 	\
	gsf.h			\
	gsf-utils.h		\
	gsf-input.h		\
	gsf-input-memory.h 	\
	gsf-input-stdio.h 	\
	gsf-infile.h 		\
	gsf-infile-msole.h 	\
	gsf-infile-zip.h	\
	gsf-output.h
	#gsf-output-memory.h 	\
	#gsf-output-stdio.h


noinst_PROGRAMS = test-msole1 test-msole2 test-zip1 test-input1
test_msole1_LDADD = libgsf-1.la $(LIBGSF_LIBS)
test_msole2_LDADD = libgsf-1.la $(LIBGSF_LIBS)
test_zip1_LDADD   = libgsf-1.la $(LIBGSF_LIBS)
test_input1_LDADD = libgsf-1.la $(LIBGSF_LIBS)

# include these just in case we are building a tarball with out gnomevfs
# support.
EXTRA_DIST = $(gnomevfs_files)
