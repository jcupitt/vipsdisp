project(
  'vipsdisp', 'c',
  version: '2.3',
  license: 'MIT',
  meson_version: '>=0.56',
  default_options: [
    'c_std=gnu99',
    # turn off asserts etc. in release mode
    'b_ndebug=if-release'
  ]
)

application_id = 'org.libvips.vipsdisp'

pkg = import('pkgconfig')
gnome = import('gnome')

cc = meson.get_compiler('c')
config_h = configuration_data()

set_defines = [
  ['APPLICATION_ID', application_id],
  ['PACKAGE', meson.project_name()],
  ['VERSION', meson.project_version()],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

configure_file(
  output: 'config.h',
  configuration: config_h,
)

vips_dep = dependency('vips', version: '>=8.11')
gtk_dep = dependency('gtk4-wayland')
m_dep = cc.find_library('m')

vipsdisp_deps = [
  vips_dep,
  gtk_dep,
  m_dep,
]

marshal = gnome.genmarshal(
  'vipsdispmarshal',
  prefix: 'vipsdisp',
  sources: 'vipsdispmarshal.list',
)

resources = gnome.compile_resources(
  'vipsdisp-gresources',
  'vipsdisp.gresources.xml',
  source_dir: 'gtk',
)

install_data(application_id + '.gschema.xml', 
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')
meson.add_install_script('meson_post_install.py')

executable('vipsdisp', [
  marshal,
  resources,
  'displaybar.c',
  'displaybar.h',
  'gtkutil.c',
  'gtkutil.h',
  'imagedisplay.c',
  'imagedisplay.h',
  'imagewindow.c',
  'imagewindow.h',
  'infobar.c',
  'infobar.h',
  'main.c',
  'tile.c',
  'tilecache.c',
  'tilecache.h',
  'tile.h',
  'tilesource.c',
  'tilesource.h',
  'tslider.c',
  'tslider.h',
  'vipsdispapp.c',
  'vipsdispapp.h',
  'vipsdisp.h',
],
  dependencies: vipsdisp_deps,
  install: true,
)


# still to do ... we had these lines in Makefile.am
#
# vipsdispdir = $(datadir)/icons/hicolor/128x128/apps
# vipsdispappdir = $(datadir)/applications
# vipsdispappdatadir = $(datadir)/metainfo
#
# vipsdisp_DATA = org.libvips.vipsdisp.png
# vipsdispapp_DATA = org.libvips.vipsdisp.desktop
# vipsdispappdata_DATA = org.libvips.vipsdisp.appdata.xml



