bin_PROGRAMS = vipsdisp

vipsdisp_SOURCES = \
	conversion.c \
	conversion.h \
	conversionview.c \
	conversionview.h \
	infobar.c \
	infobar.h \
	gtkutil.c \
	gtkutil.h \
	imagedisplay.c \
	imagedisplay.h \
	imagewindow.c \
	imagewindow.h \
	main.c \
	tslider.c \
	tslider.h \
	vipsdispapp.c \
	vipsdispapp.h \
	vipsdisp-gresources.c \
	vipsdisp.n 

AM_CPPFLAGS = @PKGFLAGS_CFLAGS@
LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@ -lm
AM_LDFLAGS = @LDFLAGS@ 

vipsdispdir = $(datadir)/icons/hicolor/512x512/apps
vipsdispappdir = $(datadir)/applications

vipsdisp_DATA = vipsdisp.png
vipsdispapp_DATA = vipsdisp.desktop

resource_files = \
	vipsdisp.gresources.xml 

vipsdisp-gresources.c: $(resource_files)
	glib-compile-resources --target=$@ --generate-source $<

CLEANFILES = vipsdisp-gresources.c

EXTRA_DIST = \
	gtk \
	$(resource_files) \
	vipsdisp.desktop.in


