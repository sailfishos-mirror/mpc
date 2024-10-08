# Copyright (C) 2008, 2009, 2010, 2011, 2012, 2014, 2016, 2017, 2018, 2020, 2022 INRIA
#
# This file is part of GNU MPC.
#
# GNU MPC is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# GNU MPC is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
# more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see http://www.gnu.org/licenses/ .


#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mpc],[1.3.2dev],[mpc-discuss@inria.fr])
AC_CONFIG_SRCDIR([src/mpc-impl.h.in])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

USER_CC=$CC
USER_CFLAGS=$CFLAGS

AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])


# Extra arguments to configure
AC_ARG_WITH([mpfr_include],
            [AS_HELP_STRING([--with-mpfr-include=DIR],
                            [MPFR include directory])],
            [CPPFLAGS="-I$withval $CPPFLAGS"])
AC_ARG_WITH([mpfr_lib],
            [AS_HELP_STRING([--with-mpfr-lib=DIR],
                            [MPFR lib directory])],
            [LDFLAGS="-L$withval $LDFLAGS"])
AC_ARG_WITH([mpfr],
            [AS_HELP_STRING([--with-mpfr=DIR],
                           [MPFR install directory])],
            [
            if test -z "$with_mpfr_include" -a -z "$with_mpfr_lib" ; then
               CPPFLAGS="-I$withval/include $CPPFLAGS"
               LDFLAGS="-L$withval/lib $LDFLAGS"
            else
               AC_MSG_FAILURE([Do not use --with-mpfr and --with-mpfr-include/--with-mpfr-lib options simultaneously.])
            fi
            ])
AC_ARG_WITH([gmp_include],
            [AS_HELP_STRING([--with-gmp-include=DIR],
                            [GMP include directory])],
            [CPPFLAGS="-I$withval $CPPFLAGS"])
AC_ARG_WITH([gmp_lib],
            [AS_HELP_STRING([--with-gmp-lib=DIR],
                            [GMP lib directory])],
            [LDFLAGS="-L$withval $LDFLAGS"])
AC_ARG_WITH([gmp],
            [AS_HELP_STRING([--with-gmp=DIR],
                           [GMP install directory])],
            [
            if test -z "$with_gmp_lib" -a -z "$with_gmp_include" ; then
               CPPFLAGS="-I$withval/include $CPPFLAGS"
               LDFLAGS="-L$withval/lib $LDFLAGS"
            else
               AC_MSG_FAILURE([Do not use --with-gmp and --with-gmp-include/--with-gmp-lib options simultaneously.])
            fi
            ])
AC_ARG_ENABLE([logging],
             [AS_HELP_STRING([--enable-logging],
                             [enable logging of function calls to stderr (default = no)])],
             [case $enableval in
                 yes) AS_IF([test "x$enable_shared" = "xno"],
                            AC_MSG_ERROR([Logging works only with shared libraries; do not mix --enable-logging and --disable-shared])) ;;
                 no)  ;;
                 *)   AC_MSG_ERROR([Bad value for --enable-logging: Use yes or no]) ;;
              esac
             ]
             )
AC_ARG_ENABLE([valgrind-tests],
             [AS_HELP_STRING([--enable-valgrind-tests],
                             [run checks through valgrind (default = no)])],
             [case $enableval in
                 yes) gl_VALGRIND_TESTS ;;
                 no)  ;;
                 *)   AC_MSG_ERROR([Bad value for --enable-valgrind-tests: Use yes or no]) ;;
              esac
             ]
             )

# Check for CC and CFLAGS in gmp.h unless the user specified one of them
# look for EGREP and SED here, see continued problem
# at https://sympa.inria.fr/sympa/arc/mpc-discuss/2012-01/msg00001.html
AC_PROG_EGREP
AC_PROG_SED
if test -z "$USER_CC" && test -z "$USER_CFLAGS"; then
   MPC_GMP_CC_CFLAGS
fi

# Setup CC and CFLAGS
AC_PROG_CC
AC_LANG(C)

# automake 1.12 seems to require this, but automake 1.11 doesn't recognize it
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Set up LibTool
LT_INIT(win32-dll)

# Check GMP Header
AC_MSG_CHECKING(for gmp.h)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include "gmp.h"
]])],[AC_MSG_RESULT(yes)],[
 AC_MSG_RESULT(no)
 AC_MSG_ERROR([gmp.h cannot be found or is unusable.])
])

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdint.h sys/time.h])
MPC_COMPLEX_H

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for libraries.
AC_CHECK_FUNCS([gettimeofday localeconv setlocale getrusage])
AC_CHECK_FUNCS([dup dup2],,
        [AC_DEFINE([MPC_NO_STREAM_REDIRECTION],1,[Do not check mpc_out_str on stdout])])

# Warning: up from autoconf 2.70, the following test fails with C++
# see https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=992981
AC_CHECK_LIB([gmp], [__gmpz_init],
             [LIBS="-lgmp $LIBS"],
             [AC_MSG_ERROR([libgmp not found or uses a different ABI (including static vs shared).])])

AC_MSG_CHECKING(for MPFR)
LIBS="-lmpfr $LIBS"
AC_LINK_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include "mpfr.h"]],
                [[mpfr_t x;  mpfr_init(x) ; mpfr_clear(x);]]
        )],
        [AC_MSG_RESULT(yes)],
        [
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([libmpfr not found or uses a different ABI (including static vs shared).])
        ])

# Check for a recent GMP
# We only guarantee that with a *functional* and recent enough GMP version,
# MPC will compile; we do not guarantee that GMP will compile.
# In particular fat builds are broken in GMP 5.0.0
# (at least on 64-bit Core 2 under GNU/Linux),
# see http://gmplib.org/list-archives/gmp-bugs/2011-August/002345.html.
AC_MSG_CHECKING(for recent GMP)
AC_COMPILE_IFELSE(
        [AC_LANG_SOURCE(
        [[
#include "gmp.h"
#if (__GNU_MP_VERSION*100 + __GNU_MP_VERSION_MINOR*10 + __GNU_MP_VERSION_PATCHLEVEL < 500)
# error "Minimal GMP version is 5.0.0"
error
#endif
        ]])],
        [AC_MSG_RESULT(yes)],
        [
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([GMP version >= 5.0.0 required])
        ])

# Check for a recent MPFR: we require MPFR 4.1.0
# The same remark as above for GMP applies.
AC_MSG_CHECKING(for recent MPFR)
AC_COMPILE_IFELSE(
        [AC_LANG_SOURCE(
        [[
#include "mpfr.h"
#if (MPFR_VERSION < MPFR_VERSION_NUM (4,1,0))
# error "Minimal MPFR version is 4.1.0"
error
#endif
        ]])],
        [AC_MSG_RESULT(yes)],
        [
         AC_MSG_RESULT(no)
         AC_MSG_ERROR([MPFR version >= 4.1.0 required])
         ])

# Check for logging feature
AS_IF([test "x$enable_logging" = "xyes"],
       [AC_CHECK_HEADERS([dlfcn.h])
        AC_CHECK_LIB([dl],[dlsym],
           [
            AC_DEFINE(HAVE_LIBDL, 1, [Library dl present])
            LIBS="-ldl $LIBS"
            AC_TYPE_INTPTR_T
            AC_LIBOBJ([logging])
            AC_SUBST([MPC_LOG_H],[src/mpc-log.h])
           ],
           [AC_MSG_ERROR([Library dl not found, logging impossible])])
       ]
     )

# Configs for Windows DLLs
AC_SUBST(MPC_LDFLAGS)
case $host in
  *-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2*)
     MPC_WINDOWS
esac

# Checks for gcc version, result in variables MPC_GCC etc. inside config.log
AX_GCC_VERSION
AC_DEFINE_UNQUOTED([MPC_GCC], ["$GCC"], [Gcc yes or no])
AC_DEFINE_UNQUOTED([MPC_CC], ["$CC"], [C compiler])
AC_DEFINE_UNQUOTED([MPC_GCC_VERSION], ["$GCC_VERSION"], [Version of gcc])

# Looks for short git hash if the version string contains "dev"
MPC_GITVERSION

# If the environment variable MPC_DEBUG is non-empty, add compiler flags.
if test $MPC_DEBUG ; then
   CFLAGS="$CFLAGS -Werror"
fi

AC_CONFIG_FILES([src/mpc.h src/mpc-impl.h Makefile src/Makefile tests/Makefile doc/Makefile tools/Makefile tools/bench/Makefile tools/mpcheck/Makefile])
AC_OUTPUT
