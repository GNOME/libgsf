INCLUDES = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)

check_PROGRAMS = test-msole1 test-msole2 test-cp-msole test-msvba	\
		 test-zip1 test-input1 test-gzip1			\
		 test-textline test-ls-zip test-cat-zip test-zip2 	\
		 test-cp-zip test-out-printf test-outmem-printf   	\
		 test-msole-printf test-zip-out test-zip-out-subdirs    \
		 test-out-gzip test-cp test-out-bzip test-bzip

ldadd = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

test_cp_LDADD		= $(ldadd)
test_msole1_LDADD	= $(ldadd)
test_msole2_LDADD	= $(ldadd)
test_cp_msole_LDADD	= $(ldadd)
test_msvba_LDADD	= $(ldadd)
test_zip1_LDADD	= $(ldadd)
test_zip2_LDADD	= $(ldadd)
test_input1_LDADD	= $(ldadd)
test_gzip1_LDADD	= $(ldadd)
test_textline_LDADD	= $(ldadd)
test_ls_zip_LDADD	= $(ldadd)
test_cat_zip_LDADD	= $(ldadd)
test_cp_zip_LDADD	= $(ldadd)
test_out_printf_LDADD	= $(ldadd)
test_outmem_printf_LDADD = $(ldadd)
test_msole_printf_LDADD	= $(ldadd)
test_zip_out_LDADD      = $(ldadd)
test_zip_out_subdirs_LDADD = $(ldadd)
test_out_gzip_LDADD	= $(ldadd)
test_out_bzip_LDADD    = $(ldadd)
test_bzip_LDADD    = $(ldadd)
#test_trans_LDADD	= $(ldadd)
