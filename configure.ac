dnl process with autoconf to produce a 'configure' script

AC_INIT([vipsdisp], [1.0])

# foreign stops complaints about a missing README (we use README.md instead)
# and missing INSTALL (the standard Gnu INSTALL is not very useful)
AM_INIT_AUTOMAKE([-Wall foreign])

GLIB_GSETTINGS

# we want largefile support, if possible
AC_SYS_LARGEFILE

AC_PROG_CC

PKG_CHECK_MODULES(PKGFLAGS, gtk4-x11 vips >= 8.1)
AC_SUBST(PKGFLAGS_CFLAGS)
AC_SUBST(PKGFLAGS_LIBS)

AC_CONFIG_FILES(org.libvips.vipsdisp.desktop)

AC_OUTPUT(Makefile)
