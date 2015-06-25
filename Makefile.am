SUBDIRS = bp

bin_PROGRAMS = vipsdisp

vipsdisp_SOURCES = \
	disp.c \
	disp.h \
	window.c \
	window.h

AM_CPPFLAGS = @PKGFLAGS_CFLAGS@
LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@
AM_LDFLAGS = @LDFLAGS@

resource_files = \
	disp.gresources.xml \
	gtk/menus.ui

disp-gresources.c: $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source $<

CLEANFILES = disp-gresources.c

EXTRA_DIST = $(resource_files)


