AUTOMAKE_OPTIONS = gnu 1.4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgsf-1.pc

if WITH_PYTHON
PYTHON_DIR = python
else
PYTHON_DIR = 
endif

SUBDIRS = gsf doc tests $(PYTHON_DIR)

#small hack to get distcheck to work
clean-local:
	rm -f intl/po2tbl.sed

EXTRA_DIST = 		\
    autogen.sh 		\
    libgsf-1.spec	\
    libgsf-1.spec.in	\
    libgsf-1.pc.in	\
    README TODO HACKING NEWS BUGS ChangeLog
