dnl
dnl Remove the first set of 'dnl's if you have automake >= 1.5.
dnl
dnl dnl a macro to check for ability to create python extensions
dnl dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl dnl function also defines PYTHON_INCLUDES
dnl AC_DEFUN([AM_CHECK_PYTHON_HEADERS],
dnl [ac_require([AM_PATH_PYTHON])	# cruft case to avoid autoconf looking at this even though it is commented out
dnl AC_MSG_CHECKING(for headers required to compile python extensions)
dnl dnl deduce PYTHON_INCLUDES
dnl py_prefix=`$PYTHON -c "import sys; print sys.prefix"`
dnl py_exec_prefix=`$PYTHON -c "import sys; print sys.exec_prefix"`
dnl PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
dnl if test "$py_prefix" != "$py_exec_prefix"; then
dnl   PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
dnl fi
dnl AC_SUBST(PYTHON_INCLUDES)
dnl dnl check if the headers exist:
dnl save_CPPFLAGS="$CPPFLAGS"
dnl CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
dnl AC_TRY_CPP([#include <Python.h>],dnl
dnl [AC_MSG_RESULT(found)
dnl $1],dnl
dnl [AC_MSG_RESULT(not found)
dnl $2])
dnl CPPFLAGS="$save_CPPFLAGS"
dnl ])
