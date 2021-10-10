bin_PROGRAMS = vipsdisp

vipsdisp_SOURCES = \
	displaybar.c \
	displaybar.h \
	infobar.c \
	infobar.h \
	gtkutil.c \
	gtkutil.h \
	imagedisplay.c \
	imagedisplay.h \
	imagewindow.c \
	imagewindow.h \
	main.c \
	tile.c \
	tile.h \
	tilesource.c \
	tilesource.h \
	tilecache.c \
	tilecache.h \
	tslider.c \
	tslider.h \
	vipsdispapp.c \
	vipsdispapp.h \
	vipsdisp-gresources.c \
	vipsdispmarshal.h \
	vipsdispmarshal.c \
	vipsdisp.h

vipsdispmarshal.h:
	glib-genmarshal \
		--prefix=vipsdisp \
		--header vipsdispmarshal.list > vipsdispmarshal.h
vipsdispmarshal.c:
	echo "#include \"vipsdispmarshal.h\"" > vipsdispmarshal.c
	glib-genmarshal \
		--prefix=vipsdisp \
		--body vipsdispmarshal.list >> vipsdispmarshal.c

AM_CPPFLAGS = @PKGFLAGS_CFLAGS@
LDADD = @PKGFLAGS_CFLAGS@ @PKGFLAGS_LIBS@ -lm
AM_LDFLAGS = @LDFLAGS@ 

vipsdispdir = $(datadir)/icons/hicolor/128x128/apps
vipsdispappdir = $(datadir)/applications
vipsdispappdatadir = $(datadir)/metainfo

vipsdisp_DATA = org.libvips.vipsdisp.png
vipsdispapp_DATA = org.libvips.vipsdisp.desktop
vipsdispappdata_DATA = org.libvips.vipsdisp.appdata.xml
gsettings_SCHEMAS = org.libvips.vipsdisp.gschema.xml
@GSETTINGS_RULES@

resource_files = \
	vipsdisp.gresources.xml 

vipsdisp-gresources.c: $(resource_files)
	glib-compile-resources \
		--target=$@ \
		--sourcedir=$(top_srcdir)/gtk \
		--generate-source \
		$<

CLEANFILES = vipsdisp-gresources.c

EXTRA_DIST = \
	gtk \
	patches \
	$(resource_files) \
	org.libvips.vipsdisp.desktop.in


