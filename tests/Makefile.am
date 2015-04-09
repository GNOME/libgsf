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
TESTS =	t1000-zip-single.pl				\
	t1001-zip-multiple.pl				\
	t1002-zip-aaaa.pl				\
	t1003-zip-noise.pl				\
	t1004-zip-zip64.pl				\
	t1005-zip-nonseekable.pl			\
	t8000-valgrind-zip.pl				\
	t8020-valgrind-ole.pl				\
	t9999-epilogue.pl

SUPPS = common.supp

AM_CPPFLAGS = -I$(top_srcdir) $(LIBGSF_CFLAGS) $(LIBXML_CFLAGS)
LDADD = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)

check_PROGRAMS = test-msole1 test-msole2 test-cp-msole test-msvba	\
		 test-msole-dup						\
		 test-zip1 test-input1 test-cp				\
		 test-textline test-ls-zip test-cat-zip test-zip2 	\
		 test-cp-zip test-out-printf test-outmem-printf   	\
		 test-msole-printf test-zip-out test-zip-out-subdirs    \
		 test-out-gzip1 test-out-gzip2 test-gzip1 test-gzip2	\
		 test-out-bzip test-bzip				\
		 test-dump-msole test-restore-msole test-msvba-zip	\
		 test-http test-gio test-xml

EXTRA_DIST = LibGsfTest.pm $(SUPPS) $(TESTS)
.PHONY: $(TESTS)
.NOTPARALLEL:
