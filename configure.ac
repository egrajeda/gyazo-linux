#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([gyazo-linux], [1.0], [tatofoo@gmail.com])
AC_CONFIG_SRCDIR([src/gyazolinux.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
PKG_CHECK_MODULES(GYAZOLINUX, [
    gtk+-2.0 >= 2.20
    gdk-2.0 >= 2.20
    libcurl >= 7.20
])

AC_CONFIG_FILES([
    Makefile 
    src/Makefile 
    data/Makefile
    data/art/Makefile
])
AC_OUTPUT
