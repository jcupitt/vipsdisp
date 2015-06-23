AM_CPPFLAGS = \
	-DSRCDIR=\"$(abs_srcdir)\"	\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	@PKGFLAGS_CFLAGS@

LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@

AM_LDFLAGS = @LDFLAGS@

noinst_PROGRAMS = bloatpad


bloatpad_SOURCES = bloatpad.c
nodist_bloatpad_SOURCES = bloatpad-gresources.c

resource_files = \
	bloatpad.gresources.xml		\
	gtk/menus.ui

bloatpad-gresources.c: $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source $<

CLEANFILES = bloatpad-gresources.c

EXTRA_DIST = $(resource_files)

