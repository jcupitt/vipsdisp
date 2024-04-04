marshal = gnome.genmarshal(
    'vipsdispmarshal',
    prefix: 'vipsdisp',
    sources: 'gtk/vipsdispmarshal.list',
)

resources = gnome.compile_resources(
    'vipsdisp-gresources',
    'gtk/vipsdisp.gresources.xml',
    source_dir: 'gtk',
)

headers = files (
    'displaybar.h',
    'fuzzy.h',
    'gtkutil.h',
    'imagedisplay.h',
    'imageui.h',
    'imagewindow.h',
    'infobar.h',
    'properties.h',
    'saveoptions.h',
    'tilecache.h',
    'tile.h',
    'tilesource.h',
    'tslider.h',
    'vipsdispapp.h',
    'vipsdisp.h',
    'vipsdispmarshal.h',
)

sources = files (
    'displaybar.c',
    'gtkutil.c',
    'imagedisplay.c',
    'imagewindow.c',
    'imageui.c',
    'infobar.c',
    'main.c',
    'tile.c',
    'tilecache.c',
    'tilesource.c',
    'tslider.c',
    'vipsdispapp.c',
    'saveoptions.c',
    'fuzzy.c',
    'properties.c',
)

enumtypes = gnome.mkenums(
    'enumtypes',
    sources: headers,
    h_template: 'enumtypes.h.in',
    c_template: 'enumtypes.c.in',
)

executable('vipsdisp', [
        enumtypes,
        marshal,
        resources,
        sources,
    ],
    dependencies: vipsdisp_deps,
    win_subsystem: 'windows',
    install: true,
)
