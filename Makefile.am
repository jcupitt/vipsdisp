SUBDIRS = bp

bin_PROGRAMS = vipsdisp

vipsdisp_SOURCES = \
	disp-gresources.c \
	disp.c \
	disp.h \
	imageview.c \
	imageview.h \
	imagepresent.c \
	imagepresent.h \
	conversion.c \
	conversion.h \
	imagedisplay.c \
	imagedisplay.h

AM_CPPFLAGS = @PKGFLAGS_CFLAGS@
LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@
AM_LDFLAGS = @LDFLAGS@

resource_files = \
	disp.gresources.xml \
	gtk/imageview-popover.ui \
	gtk/menus.ui

disp-gresources.c: $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source $<

CLEANFILES = disp-gresources.c

EXTRA_DIST = $(resource_files)


