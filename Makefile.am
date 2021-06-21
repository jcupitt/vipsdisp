bin_PROGRAMS = vipsdisp

vipsdisp_SOURCES = \
	vipsdisp.n \
	conversion.c \
	conversion.h \
	imagedisplay.c \
	imagedisplay.h \
	vipsdisp-gresources.c \
	imagewindow.c \
	imagewindow.h \
	main.c \
	vipsdispapp.c \
	vipsdispapp.h

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
	glib-compile-resources --target=$@ --sourcedir=gtk --generate-source $<

CLEANFILES = vipsdisp-gresources.c

EXTRA_DIST = \
	gtk \
	$(resource_files) \
	vipsdisp.desktop.in


