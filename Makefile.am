bin_PROGRAMS = vd

vd_SOURCES = \
	disp-gresources.c \
	settings.c \
	disp.c \
	disp.h \
	gtkutil.h \
	gtkutil.c \
	tslider.h \
	tslider.c \
	infobar.c \
	infobar.h \
	imageview.c \
	imageview.h \
	imagepresent.c \
	imagepresent.h \
	conversion.c \
	conversion.h \
	conversionview.c \
	conversionview.h \
	imagedisplay.c \
	imagedisplay.h

AM_CPPFLAGS = @PKGFLAGS_CFLAGS@
LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@ -lm
AM_LDFLAGS = @LDFLAGS@ 

vddir = $(datadir)/icons/hicolor/512x512/apps
vd_DATA = vipsdisp-icon.png

resource_files = \
	disp.gresources.xml \
	gtk/conversionview-popover.ui \
	gtk/imageview-popover.ui 

disp-gresources.c: $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source $<

CLEANFILES = disp-gresources.c

EXTRA_DIST = $(resource_files)


