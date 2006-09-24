AM_CPPFLAGS = $(LIBGSF_CFLAGS) $(PYTHON_INCLUDES) $(PYGTK_CFLAGS)

pygsfdir = $(pythondir)/gsf
pygsf_PYTHON = __init__.py

pygsfexecdir = $(pyexecdir)/gsf

CLEANFILES =
EXTRA_DIST =
pygsfexec_LTLIBRARIES =

if WITH_LIBGSF_GNOME
  pygsfexec_LTLIBRARIES += gnomemodule.la
endif

pygsfexec_LTLIBRARIES += _gsfmodule.la

gnomemodule_la_CFLAGS = $(LIBGSF_CFLAGS) $(LIBGSF_GNOME_CFLAGS) $(ORBIT_PYTHON_CFLAGS)
gnomemodule_la_LDFLAGS = -module -avoid-version -export-symbols-regex initgnome
gnomemodule_la_LIBADD = \
	$(top_builddir)/gsf-gnome/libgsf-gnome-1.la	\
	$(top_builddir)/gsf/libgsf-1.la			\
	$(LIBGSF_LIBS) $(LIBGSF_GNOME_LIBS)
gnomemodule_la_SOURCES = gsfgnomemodule.c
nodist_gnomemodule_la_SOURCES = gsfgnome.c
CLEANFILES += gsfgnome.c
EXTRA_DIST += gsfgnome.defs gsfgnome.override  
gsfgnome.c: gsfgnome.defs gsfgnome.override

_gsfmodule_la_CFLAGS = $(LIBGSF_CFLAGS)
_gsfmodule_la_LDFLAGS = -module -avoid-version -export-symbols-regex init_gsf
_gsfmodule_la_LIBADD = $(top_builddir)/gsf/libgsf-1.la $(LIBGSF_LIBS)
_gsfmodule_la_SOURCES = gsfmodule.c
nodist__gsfmodule_la_SOURCES = gsf.c
CLEANFILES += gsf.c
EXTRA_DIST += gsf.defs gsf.override  
gsf.c: gsf.defs gsf.override

.defs.c:
	(cd $(srcdir) \
	 && $(PYGTK_CODEGEN) \
	    --load-types gsf-arg-types.py \
	    --override $*.override \
	    --prefix py$* $*.defs) > gen-$*.c \
	&& cp gen-$*.c $*.c \
	&& rm -f gen-$*.c
