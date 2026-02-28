AM_CPPFLAGS = -I$(top_srcdir) $(LIBGSF_CFLAGS) \
	$(Z_CPPFLAGS) \
	-DGSFLOCALEDIR=\"$(localedir)\"
LDADD = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

bin_PROGRAMS = gsf gsf-vba-dump
noinst_PROGRAMS = gsf-detect-bad-msole-encoding
gsf_SOURCES = gsf.c
gsf_vba_dump_SOURCES = gsf-vba-dump.c
gsf_detect_bad_msole_encoding_SOURCES = gsf-detect-bad-msole-encoding.c
