#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([maefat], [1.1], [orrery.moko@gmail.com],[maefat],[http://wiki.maemo.org/maeFat])
AM_INIT_AUTOMAKE([-Wall])
AC_COPYRIGHT([GPL])
AC_CONFIG_SRCDIR([src/maeFat.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([m], [pow])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h math.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([pow])

# Check for libraries:
PKG_CHECK_MODULES(GTK, [gtk+-2.0])
PKG_CHECK_MODULES(HILDON, [hildon-1])
PKG_CHECK_MODULES(FM, [hildon-fm-2])
PKG_CHECK_MODULES(LIBOSSO, [libosso])
PKG_CHECK_MODULES(DBUS, [dbus-1])

AC_CONFIG_FILES([Makefile src/Makefile maemoFiles/Makefile])
AC_OUTPUT
