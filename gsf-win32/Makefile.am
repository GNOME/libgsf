INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS)

if PLATFORM_WIN32
target_libgsf_win32 = libgsf-win32-1.la
else
target_libgsf_win32 =
endif

lib_LTLIBRARIES = $(target_libgsf_win32)

libgsf_win32_1_la_LIBADD  = $(top_builddir)/gsf/libgsf-1.la
libgsf_win32_1_la_LDFLAGS = -version-info $(VERSION_INFO)
libgsf_win32_1_la_SOURCES =	\
	gsf-input-win32.c	\
	gsf-output-win32.c

libgsf_win32_1_includedir = $(includedir)/libgsf-1/gsf-win32
libgsf_win32_1_include_HEADERS =  \
	gsf-input-win32.h	\
	gsf-output-win32.h

# include these just in case we are building a tarball without win32 support
EXTRA_DIST = $(libgsf_win32_1_la_SOURCES) $(libgsf_win32_1_include_HEADERS)
