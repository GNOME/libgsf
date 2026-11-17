# First digit after "t" the type of test:
#
# 1xxx: Containers
# 10xx:   zip
# 11xx:   ole2
# 12xx:   tar
#
# 8xxx: Runs with valgrind and similar tools.
# 80xx:   Valgrind
# 82xx:   G_SLICE debug and similar
#
# 90xx: Misc stuff.

TESTS_EXTENSIONS=.pl
check_LTLIBRARIES = libregress.la
libregress_la_SOURCES = libgsf-regress.c libgsf-regress.h

TSCRIPTS = \
	t1000-zip-single.pl				\
	t1001-zip-multiple.pl				\
	t1002-zip-aaaa.pl				\
	t1003-zip-noise.pl				\
	t1004-zip-zip64.pl				\
	t1005-zip-nonseekable.pl			\
	t1100-ole.pl					\
	t1101-ole-many.pl				\
	t2000-iconv.pl					\
	t2001-csv.pl					\
	t2002-compress.pl				\
	t8000-valgrind-zip.pl				\
	t8020-valgrind-ole.pl				\
	t8050-valgrind-regress.pl			\
	t1200-cli.pl					\
	t9999-epilogue.pl

TESTS = $(TSCRIPTS) test-xml-fixes			\
	test-regress-blob test-regress-tar test-regress-zipdup		\
	test-regress-iconv test-regress-compress test-regress-textline	\
	test-regress-timestamp test-regress-meta test-regress-msvba	\
	test-regress-msole test-regress-output test-regress-input	\
	test-regress-zip test-regress-stdio test-regress-iochannel \
	test-regress-shared-memory test-regress-docprop-vector \
	test-regress-doc-meta-data test-regress-xmlin-unknown	\
	test-regress-gio test-regress-gzip-setup test-regress-msole-vt-cy	\
	test-regress-structured-blob

SUPPS = common.supp

# White-box test: includes gsf-msole-utils.c to reach a static function.
test_regress_msole_vt_cy_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_builddir) -I$(top_srcdir)/gsf

AM_CPPFLAGS = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)
LDADD = libregress.la $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

check_PROGRAMS = test-msole1 test-msole2 test-cp-msole test-msvba	\
		 test-msole-dup						\
		 test-zip1 test-input1 test-cp				\
		 test-textline test-ls-zip test-cat-zip test-zip2 	\
		 test-cp-zip test-out-printf test-outmem-printf   	\
		 test-msole-printf test-zip-out test-zip-out-subdirs    \
		 test-out-gzip1 test-out-gzip2 test-gzip1 test-gzip2	\
		 test-out-bzip test-bzip				\
		 test-dump-msole test-restore-msole test-msvba-zip	\
		 test-http test-gio test-xml test-ole test-iconv test-csv test-compress	\
		 test-xml-fixes						\
		 test-regress-blob test-regress-tar test-regress-zipdup	\
		 test-regress-iconv test-regress-compress		\
		 test-regress-textline test-regress-timestamp		\
		 test-regress-meta test-regress-msvba test-regress-msole \
		 test-regress-outmem-printf test-regress-xmlout-uaf	\
		 test-regress-infile-leak test-regress-stdio		\
		 test-regress-outzip-open				\
		 test-regress-output test-regress-input test-regress-zip	\
		 test-regress-iochannel test-regress-shared-memory	\
		 test-regress-docprop-vector test-regress-doc-meta-data	\
		 test-regress-xmlin-unknown test-regress-gio		\
		 test-regress-gzip-setup test-regress-msole-vt-cy	\
		 test-regress-structured-blob

EXTRA_DIST = LibGsfTest.pm $(SUPPS) $(TSCRIPTS)

.NOTPARALLEL:
