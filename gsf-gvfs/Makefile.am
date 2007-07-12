AM_CPPFLAGS = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBGSF_GVFS_CFLAGS)

if WITH_GVFS

lib_LTLIBRARIES = libgsf-gvfs-1.la

libgsf_gvfs_1_la_LIBADD  = $(LIBGSF_LIBS) $(LIBGSF_GVFS_LIBS) $(top_builddir)/gsf/libgsf-1.la
libgsf_gvfs_1_la_LDFLAGS = -version-info $(VERSION_INFO) -no-undefined
libgsf_gvfs_1_la_SOURCES =	\
	gsf-input-gvfs.c	\
	gsf-output-gvfs.c

endif

## But headers are installed on all systems:
libgsf_gvfs_1_includedir = $(includedir)/libgsf-1/gsf-gvfs
libgsf_gvfs_1_include_HEADERS =  \
	gsf-input-gvfs.h	 \
	gsf-output-gvfs.h
