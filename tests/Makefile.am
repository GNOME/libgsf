INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)

check_PROGRAMS = test-msole1 test-msole2 test-cp-msole test-msvba	\
		 test-zip1 test-input1 test-gzip1			\
		 test-textline

ldadd = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

test_msole1_LDADD	= $(ldadd)
test_msole2_LDADD	= $(ldadd)
test_cp_msole_LDADD	= $(ldadd)
test_msvba_LDADD	= $(ldadd)
test_zip1_LDADD		= $(ldadd)
test_input1_LDADD	= $(ldadd)
test_gzip1_LDADD	= $(ldadd)
test_textline_LDADD	= $(ldadd)
