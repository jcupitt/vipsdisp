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

if target_machine.system() == 'windows'
  windows = import('windows')
  # this picks up the windres binary from the WINDRES env var
  resources += windows.compile_resources('vipsdisp.rc', 
    args: ['-O', 'coff'], 
    depend_files: 'vipsdisp.ico')
endif

headers = files (
    'displaybar.h',
    'fuzzy.h',
    'gtkutil.h',
    'ientry.h',
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
    'fuzzy.c',
    'gtkutil.c',
    'ientry.c',
    'imagedisplay.c',
    'imageui.c',
    'imagewindow.c',
    'infobar.c',
    'main.c',
    'properties.c',
    'saveoptions.c',
    'tile.c',
    'tilecache.c',
    'tilesource.c',
    'tslider.c',
    'vipsdispapp.c',
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
