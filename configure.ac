dnl process with autoconf to produce a 'configure' script

AC_INIT([vipsdisp], [1.0])

# foreign stops complaints about a missing README (we use README.md instead)
# and missing INSTALL (the standard Gnu INSTALL is not very useful)
AM_INIT_AUTOMAKE([-Wall foreign])

# we want largefile support, if possible
AC_SYS_LARGEFILE

AC_PROG_CC

PKG_CHECK_MODULES(PKGFLAGS, gtk+-3.0 vips >= 8.1)
AC_SUBST(PKGFLAGS_CFLAGS)
AC_SUBST(PKGFLAGS_LIBS)

AC_OUTPUT(Makefile)
