# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# $Id$

# This file is part of Polypaudio Volume Meter.
#
# Polypaudio Volume Meter is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Polypaudio Volume Meter is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Polypaudio Volume Meter; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

AC_PREREQ(2.57)
AC_INIT([pavumeter],[0.1],[mzihzrgre (at) 0pointer (dot) de])
AC_CONFIG_SRCDIR([src/vumeter.cc])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall])

AC_SUBST(PACKAGE_URL, [http://0pointer.de/lennart/projects/pavumeter/])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LN_S
AC_TYPE_SIGNAL
AC_HEADER_STDC

PKG_CHECK_MODULES(GUILIBS, [ gtkmm-2.4 ])
AC_SUBST(GUILIBS_CFLAGS)
AC_SUBST(GUILIBS_LIBS)

if test -d ../polypaudio ; then
   POLYP_CFLAGS='-I$(top_srcdir)/../polypaudio'
   POLYP_LIBS='-L$(top_srcdir)/../polypaudio/polyp/.libs -lpolyp -lpolyp-mainloop-glib -lpolyp-mainloop-glib -lpolyp-error'
   echo "*** Found polypaudio in ../polypaudio, using that version ***"
else
   PKG_CHECK_MODULES(POLYP, [ polyplib >= 0.4 polyplib-glib-mainloop >= 0.4 ])
fi

AC_SUBST(POLYP_LIBS)
AC_SUBST(POLYP_CFLAGS)

# If using GCC specifiy some additional parameters
if test "x$GCC" = "xyes" ; then
   CFLAGS="$CFLAGS -pipe -Wall -W -Wno-unused-parameter"
fi

# LYNX documentation generation
AC_ARG_ENABLE(lynx,
        AS_HELP_STRING(--disable-lynx,Turn off lynx usage for documentation generation),
[case "${enableval}" in
  yes) lynx=yes ;;
  no)  lynx=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --disable-lynx) ;;
esac],[lynx=yes])

if test x$lynx = xyes ; then
   AC_CHECK_PROG(have_lynx, lynx, yes, no)

   if test x$have_lynx = xno ; then
     AC_MSG_ERROR([*** Sorry, you have to install lynx or use --disable-lynx ***])
   fi
fi

AM_CONDITIONAL([USE_LYNX], [test "x$lynx" = xyes])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile doc/README.html])
AC_OUTPUT
