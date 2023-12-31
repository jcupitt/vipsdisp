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

executable('vipsdisp', [
    marshal,
    resources,
    'displaybar.c',
    'gtkutil.c',
    'imagedisplay.c',
    'imagewindow.c',
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
  ],
  dependencies: vipsdisp_deps,
  win_subsystem: 'windows',
  install: true,
)
