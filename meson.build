project(
  'vipsdisp', 'c',
  version: '2.6.0',
  license: 'MIT',
  meson_version: '>=0.56',
  default_options: [
    'c_std=c11',
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
# so it can find config.h
add_project_arguments('-I.', language: 'c')

configure_file(
  output: 'config.h',
  configuration: config_h,
)

# need vips_thread_execute()
vips_dep = dependency('vips', version: '>=8.15')
gtk_dep = dependency('gtk4')
m_dep = cc.find_library('m')

vipsdisp_deps = [
  vips_dep,
  gtk_dep,
  m_dep,
]

install_data(application_id + '.gschema.xml', 
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')

install_data(application_id + '.png', 
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps')
install_data(application_id + '.desktop', 
  install_dir: get_option('datadir') / 'applications')
install_data(application_id + '.metainfo.xml', 
  install_dir: get_option('datadir') / 'metainfo')

meson.add_install_script('meson_post_install.py')

subdir('src')
