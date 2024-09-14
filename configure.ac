-*- mode: m4 -*-
dnl We require Automake 1.7.1, which requires Autoconf 2.54.
AC_PREREQ([2.54])
AC_CONFIG_MACRO_DIR([m4])
AX_IS_RELEASE([git-directory])

m4_define([libgsf_version_major], [1])
m4_define([libgsf_version_minor], [14])
m4_define([libgsf_version_micro], [54])
m4_define([libgsf_version_extra], [])
m4_define([libgsf_full_version],
    [libgsf_version_major.libgsf_version_minor.libgsf_version_micro[]libgsf_version_extra])

AC_INIT([libgsf],[libgsf_full_version],[https://gitlab.gnome.org/GNOME/libgsf/issues])
AC_CONFIG_SRCDIR(gsf/gsf.h)
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS -I m4")

AC_SUBST(LIBGSF_MAJOR_VERSION, libgsf_version_major)
AC_SUBST(LIBGSF_MINOR_VERSION, libgsf_version_minor)
AC_SUBST(LIBGSF_MICRO_VERSION, libgsf_version_micro)

dnl Version info for libraries = CURRENT:REVISION:AGE
dnl
dnl Within each x.y.*, ABI is maintained backward compatible.
dnl We strive for forward compatibility too, except that we sometimes
dnl add new functions.
dnl
m4_define([version_iface],
	m4_eval(100 * libgsf_version_major + libgsf_version_minor))

AC_SUBST([VERSION_INFO], [version_iface:libgsf_version_micro:0])
AC_SUBST([CURRENT_MINUS_AGE], m4_eval(version_iface - 0))

AM_INIT_AUTOMAKE(dist-bzip2)

AC_CONFIG_HEADERS(gsf-config.h)
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl Propagate Gnome-specific variable ACLOCAL_FLAGS to Makefile.
AC_SUBST(ACLOCAL_AMFLAGS, $ACLOCAL_FLAGS)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
LT_PREREQ([2.2.6])
LT_INIT([win32-dll])

AC_FUNC_FSEEKO
AC_SYS_LARGEFILE
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

dnl Gettext/i18n stuff
# FIXME: Remove AM_GNU_GETTEXT_VERSION once autoreconf supports REQUIRE_VERSION
AM_GNU_GETTEXT_VERSION([0.19.6])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.6])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Checks for libraries.
ifelse([
  If we defined the module lists on the M4 level, we could have problems
  with PKG_CHECK_MODULES from pkgconfig 0.16.0, which double quotes its
  second argument (the module list).
  As a handy workaround, we use shell variables.
])
dnl Modules required for libgsf
libgsf_reqs="
    gobject-2.0 >= 2.62.0
    glib-2.0 >= 2.62.0
    gio-2.0 >= 2.62.0
    libxml-2.0 >= 2.4.16
"

PKG_CHECK_MODULES(LIBGSF, $libgsf_reqs)

AC_DEFINE(GLIB_VERSION_MIN_REQUIRED, [GLIB_VERSION_2_62], [GLib symbol visibility])
AC_DEFINE(GLIB_VERSION_MAX_ALLOWED, [GLIB_VERSION_2_62], [GLib symbol visibility])

# GObject Introspection
GIR_REQ=1.0.0
AC_ARG_ENABLE(introspection,
          AS_HELP_STRING([--enable-introspection[=@<:@no/auto/yes@:>@]],
                         [Enable introspection for this build]),, 
                         [enable_introspection=no])

AC_MSG_CHECKING([for gobject-introspection])

dnl presence/version checking
AS_CASE([$enable_introspection],
[no], [
    found_introspection="no (disabled, use --enable-introspection to enable)"
],
[yes],[
    PKG_CHECK_EXISTS([gobject-introspection-1.0],,
                     AC_MSG_ERROR([gobject-introspection-1.0 is not installed]))
    PKG_CHECK_EXISTS([gobject-introspection-1.0 >= $GIR_REQ],
                     found_introspection=yes,
                     AC_MSG_ERROR([You need to have gobject-introspection >= $1 installed to build AC_PACKAGE_NAME]))
],
[auto],[
    PKG_CHECK_EXISTS([gobject-introspection-1.0 >= $GIR_REQ], found_introspection=yes, found_introspection=no)
dnl Canonicalize enable_introspection
enable_introspection=$found_introspection
],
[	
    AC_MSG_ERROR([invalid argument passed to --enable-introspection, should be one of @<:@no/auto/yes@:>@])
])

AC_MSG_RESULT([$found_introspection])

if test "x$found_introspection" = "xyes"; then
   dnl You can override INTROSPECTION_GIRDIR and INTROSPECTION_TYPELIBDIR
   dnl if you wish.  If you override the latter, you might want to set
   dnl GI_TYPELIB_PATH to include the same directory.  For example
   dnl
   dnl GI_TYPELIB_PATH=$PREFIX/lib64/girepository-1.0
   dnl INTROSPECTION_TYPELIBDIR=$GI_TYPELIB_PATH
   dnl INTROSPECTION_GIRDIR=$PREFIX/share/gir-1.0
   dnl
   dnl Note, that unlike binaries produced with libgsf, nothing tells
   dnl python where to find libgsf, so you might also need to set
   dnl LD_LIBRARY_PATH.

   INTROSPECTION_SCANNER=`$PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0`
   INTROSPECTION_COMPILER=`$PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0`
   INTROSPECTION_GENERATE=`$PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0`
   if test "x$INTROSPECTION_GIRDIR" = x; then
     INTROSPECTION_GIRDIR=`$PKG_CONFIG --variable=girdir gobject-introspection-1.0`
   fi
   if test "x$INTROSPECTION_TYPELIBDIR" = x; then
     INTROSPECTION_TYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"
   fi
   INTROSPECTION_CFLAGS=`$PKG_CONFIG --cflags gobject-introspection-1.0`
   INTROSPECTION_LIBS=`$PKG_CONFIG --libs gobject-introspection-1.0`
   INTROSPECTION_MAKEFILE=`$PKG_CONFIG --variable=datadir gobject-introspection-1.0`/gobject-introspection-1.0/Makefile.introspection
else
   INTROSPECTION_SCANNER=
   INTROSPECTION_COMPILER=
   INTROSPECTION_GENERATE=
   INTROSPECTION_GIRDIR=
   INTROSPECTION_TYPELIBDIR=
   INTROSPECTION_CFLAGS=
   INTROSPECTION_LIBS=
   INTROSPECTION_MAKEFILE=
fi
AC_SUBST(INTROSPECTION_SCANNER)
AC_SUBST(INTROSPECTION_COMPILER)
AC_SUBST(INTROSPECTION_GENERATE)
AC_SUBST(INTROSPECTION_GIRDIR)
AC_SUBST(INTROSPECTION_TYPELIBDIR)
AC_SUBST(INTROSPECTION_CFLAGS)
AC_SUBST(INTROSPECTION_LIBS)
AC_SUBST(INTROSPECTION_MAKEFILE)

AM_CONDITIONAL(HAVE_INTROSPECTION, test "x$found_introspection" = "xyes")
dnl we need to change the install directories for distcheck
AC_ARG_WITH([gir-dir],
	AS_HELP_STRING(
		[--with-gir-dir],
		[
		 path to gir repository
		 (automatically detected via pkg-config)
		]
	),
	[GIRDIR=$withval],
	[GIRDIR=$INTROSPECTION_GIRDIR]
)
AC_SUBST(GIRDIR)
AC_ARG_WITH([typelib-dir],
	AS_HELP_STRING(
		[--with-typelib-dir],
		[
		 path to typelibs repository
		 (automatically detected via pkg-config)
		]
	),
	[TYPELIBDIR=$withval],
	[TYPELIBDIR=$INTROSPECTION_TYPELIBDIR]
)
AC_SUBST(TYPELIBDIR)

## this should come after `AC_PROG_CC'
set_more_warnings=yes
if test "$GCC" = yes -a "x$set_more_warnings" != xno; then
	dnl Clang needs to option, or else it will appear to support any
	dnl warning option
	uwoption="-Werror=unknown-warning-option"
	SAVE_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $uwoption"
	AC_MSG_CHECKING([whether gcc understands $uwoption])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([], [])],
		[has_unknown_warning_option=yes],
		[has_unknown_warning_option=no; uwoption=""])
	AC_MSG_RESULT($has_unknown_warning_option)
	CFLAGS="$SAVE_CFLAGS"

	warning_options="-Wall -Werror=init-self -Werror=missing-include-dirs \
			 -Wsign-compare -Werror=pointer-arith \
			 -Wchar-subscripts -Wwrite-strings \
			 -Wdeclaration-after-statement -Wnested-externs \
			 -Wmissing-noreturn \
			 -Werror=missing-prototypes -Werror=nested-externs \
			 -Werror=implicit-function-declaration \
			 -Wmissing-declarations -Wno-pointer-sign \
			 -Werror=format-security -Wbitwise -Wcast-to-as \
			 -Wdefault-bitfield-sign -Wdo-while -Wparen-string \
			 -Wptr-subtraction-blows -Wreturn-void -Wtypesign \
			 -Wstrict-prototypes -Wno-error=format-nonliteral "
	for option in $warning_options ; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $uwoption $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		dnl Include a system header so we ignore Werror=... flags
		dnl that cause trouble.
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([[#include <stdio.h>]], [])],
			[has_option=yes],
			[has_option=no])
		CFLAGS="$SAVE_CFLAGS"
		if test $has_option = yes; then
			CFLAGS="$CFLAGS $option"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
fi
AC_SUBST(WARN_CFLAGS)

AC_CHECK_DECL(fdopen, fdopen_works=yes, fdopen_works=no)
if test $fdopen_works = no ; then
	unset ac_cv_have_decl_fdopen
	CFLAGS="$CFLAGS -D_POSIX_SOURCE"
	AC_MSG_NOTICE([adding -D_POSIX_SOURCE to CFLAGS])
	AC_CHECK_DECL(fdopen, fdopen_works=yes, fdopen_works=no)
	if test $fdopen_works = no ; then
		AC_MSG_ERROR([fdopen is not available])
	fi
fi

# Unfortunately, -D_POSIX_SOURCE turns off struct timeval on Solaris
AC_MSG_CHECKING([whether struct timeval is available])
for try in 1 2; do
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[#include <sys/time.h>]],
			[[struct timeval tv;]])],
		struct_timeval_works=yes,
		struct_timeval_works=no)
	test $struct_timeval_works = yes && break
	# Try this for the second attempt:
	test $try = 1 && CFLAGS="$CFLAGS -D__EXTENSIONS__"
done
AC_MSG_RESULT($struct_timeval_works)
if test $struct_timeval_works = no ; then
	AC_MSG_ERROR([struct timeval is not available])
fi

AC_CHECK_MEMBERS([struct stat.st_mtimensec, struct stat.st_mtim.tv_nsec])

AC_MSG_CHECKING([whether -D_BSD_SOURCE is needed for caddr_t])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[#include <sys/types.h>]], [[caddr_t ca]])],
	need_bsd1=no,
	need_bsd1=yes)
AC_MSG_RESULT($need_bsd1)

AC_MSG_CHECKING([whether -D_BSD_SOURCE is needed for lstat])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>]],
[[void *ptr = &lstat]])],
need_bsd2=no, need_bsd2=yes)
AC_MSG_RESULT($need_bsd2)
if test $need_bsd1 = yes -o $need_bsd2 = yes; then
	CFLAGS="$CFLAGS -D_BSD_SOURCE"
fi

AC_TYPE_MODE_T
AC_CHECK_HEADERS(fcntl.h malloc.h unistd.h io.h sys/statfs.h sys/param.h sys/mount.h utime.h sys/utime.h)
AC_FUNC_MMAP

AC_MSG_CHECKING([form of statfs])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif]],
[[struct statfs buf; (void)(2*statfs("xyz",&buf));]])],
	[AC_MSG_RESULT([2-arg, linux-style])
AC_DEFINE(HAVE_2ARG_STATFS, 1, [Define if you have two-argument statfs like linux])],
	[AC_MSG_RESULT([none, or unknown])])

AC_CHECK_FUNCS(chown setrlimit gmtime_r gmtime)
AC_CHECK_MEMBERS(struct tm.tm_gmtoff,,,
[#include <time.h>])

SAVE_CFLAGS=$CFLAGS
SAVE_LIBS=$LIBS
CFLAGS="$CFLAGS $LIBGSF_CFLAGS"
LIBS="$LIBGSF_LIBS $LIBS"
AC_MSG_CHECKING([for g_chown])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <glib/gstdio.h>]], [[(void)g_chown("/xxx",-1,-1);]])],
               [AC_DEFINE(HAVE_G_CHOWN, 1, [Define if g_chown is available as macro or function])
		AC_MSG_RESULT(yes)],
	       [AC_MSG_RESULT(no)])
AC_CHECK_FUNCS(g_date_time_new_from_iso8601 g_date_time_format_iso8601)
AC_CHECK_FUNCS(xmlNanoHTTPOpen xmlNanoHTTPClose)
CFLAGS=$SAVE_CFLAGS
LIBS=$SAVE_LIBS

AC_MSG_CHECKING([whether macro S_ISREG is available])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#include <sys/types.h>
#include <sys/stat.h>]],
[[S_ISREG(S_IFMT);]])],
	[macro_s_isreg=yes],
	[AH_TEMPLATE([S_ISREG], [Define S_ISREG if stat.h doesn't])
	 AC_DEFINE([S_ISREG(m)], [(((m) & S_IFMT) == S_IFREG)])
	 macro_s_isreg=no])
AC_MSG_RESULT($macro_s_isreg)

AC_MSG_CHECKING([for Win32 platform])
with_win32=no
case $host_os in
  mingw* | pw32* | cygwin*)
    with_win32=yes
    AC_ARG_VAR(WINDRES, [The windres executable (used by win32 builds only).])
    AC_CHECK_TOOL(WINDRES, windres)
    AC_ARG_VAR(LIBEXE, [The lib.exe executable (used by win32 builds only).])
    AC_CHECK_PROG(LIBEXE, lib.exe, yes, no)
    ;;
esac
AC_MSG_RESULT($with_win32)
AM_CONDITIONAL(WITH_WIN32, test $with_win32 = yes)
AM_CONDITIONAL(HAVE_LIBEXE, test x$LIBEXE = xyes)
AM_CONDITIONAL(CROSS_COMPILING, test x"$cross_compiling" != "xno")

##################################################
# Checks for gtk-doc and docbook-tools
##################################################
gtk_doc_installed=true
#ifdef([GTK_DOC_CHECK],[GTK_DOC_CHECK([1.12])],[gtk_doc_installed=false])
# I would have liked to conditionalize this, but 
# that appears to break other things http://bugzilla.gnome.org/show_bug.cgi?id=156399
dnl AX_REQUIRE_DEFINED([GTK_DOC_CHECK])
GTK_DOC_CHECK([1.12])
AM_CONDITIONAL(GTK_DOC_INSTALLED, $gtk_doc_installed)

dnl ***********************************************************************************
# Check for zlib.
_cppflags=$CPPFLAGS
_ldflags=$LDFLAGS

Z_DIR= Z_LIBS= Z_CPPFLAGS=

AC_ARG_WITH(zlib,
        [[  --with-zlib=DIR       use libz in DIR]],
        [case $withval in
         yes|no) ;;
         *)     Z_DIR=$withval
                CPPFLAGS="${CPPFLAGS} -I$withval/include"
                LDFLAGS="${LDFLAGS} -L$withval/lib"
                ;;
         esac])

if test "x$with_zlib" != xno; then
        with_zlib=no
        AC_CHECK_HEADER(zlib.h, [AC_CHECK_LIB(z, gzread, [with_zlib=yes])])
fi
if test "$with_zlib" = no; then
        AC_MSG_ERROR([*** zlib is required])
fi
if test "x$Z_DIR" != "x"; then
        Z_CPPFLAGS="-I$Z_DIR/include"
        case $host in
        *-*-solaris*)   Z_LIBS="-L$Z_DIR/lib -R$Z_DIR/lib -lz" ;;
        *)              Z_LIBS="-L$Z_DIR/lib -lz" ;;
        esac
else
        Z_LIBS="-lz"
fi
AC_SUBST(Z_CPPFLAGS)
AC_SUBST(Z_LIBS)

CPPFLAGS=${_cppflags}
LDFLAGS=${_ldflags}

dnl ***********************************************************************************

dnl bz2
dnl
BZ2_LIBS=
test_bz2=true
AC_ARG_WITH(bz2,
	[--{with,without}-bz2   Build additional wrappers for BZ2],
	if test "x$withval" = xno; then
		test_bz2=false
	fi
)

with_bz2=false
if test "x$test_bz2" = xtrue ; then
   AC_MSG_CHECKING([usable libbz2])

   dnl AC_CHECK_LIB doesn't work with the Windows port of libbz2 because
   dnl it uses the __stdcall calling convention where the callee cleans
   dnl up the stack.  You have to include <bzlib.h> in order to get the
   dnl right calling convention, and the link fails if not.  Since we
   dnl require both -lbz2 and <bzlib.h>, just test if a program which
   dnl uses both can be compiled.

   old_LIBS="$LIBS"
   LIBS="-lbz2 $LIBS"
   AC_LINK_IFELSE(
     [AC_LANG_SOURCE(
	[[#include <stdio.h>
	  #undef PACKAGE
	  #undef VERSION
	  #undef HAVE_STDLIB_H
	  #include <bzlib.h>
          int main (void)
          {
            return BZ2_bzDecompressInit (NULL, 0, 0);
          }
        ]])],
     bz2_ok=yes,
     bz2_ok=no)
   LIBS="$old_LIBS"

   if test "$bz2_ok" = yes; then
	AC_DEFINE(HAVE_BZ2, 1, [Is bzip2 available and enabled])
	BZ2_LIBS="-lbz2"
	AC_MSG_RESULT($bz2_ok)
   else
	AC_MSG_WARN(*** BZ2 support disabled (BZ2 header or library not found) ***)
   fi
else
   AC_MSG_WARN([BZ2 support disabled, as requested (Use --with-bz2 to enable)])
fi

AC_SUBST(BZ2_LIBS)

dnl ****************************************************************************

want_gdk_pixbuf=no
dnl check for gdk_pixbuf
AC_ARG_WITH(gdk_pixbuf,
    [AS_HELP_STRING([--without-gdk-pixbuf],[disable the use of gdk-pixbuf])],
    [case "${withval}" in
    yes) want_gdk_pixbuf=yes ;;
    no) want_gdk_pixbuf=no ;;
    *) AC_MSG_ERROR(bad value ${withval} for --with-gdk-pixbuf) ;;
    esac],
    [want_gdk_pixbuf=auto]) dnl Default value

if test $want_gdk_pixbuf = no ; then
    AC_MSG_WARN([gdk-pixbuf support disabled, as requested (Use --with-gdk-pixbuf to enable)])
else
    PKG_CHECK_MODULES(GDK_PIXBUF, gdk-pixbuf-2.0,
        [AC_DEFINE(HAVE_GDK_PIXBUF, 1, [Define if gdk-pixbuf is available])],
	[if test $want_gdk_pixbuf = yes; then
	    AC_MSG_ERROR([gdk-pixbuf support requested, but not available.])
	 else
	    AC_MSG_WARN([gdk-pixbuf support disabled, unable to find required version])
	 fi])
fi

# ===================

LDFLAGS="-no-undefined $LDFLAGS"

AC_CONFIG_FILES([
Makefile
doc/Makefile
gsf/Makefile
gsf-win32/Makefile
python/Makefile
tests/Makefile
tools/Makefile
thumbnailer/Makefile
po/Makefile.in
gsf/version.c
libgsf-1.pc
libgsf-win32-1.pc
libgsf-zip
])
AC_OUTPUT

AC_MSG_RESULT([Configured $PACKAGE $VERSION])
