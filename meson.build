project(
  'anvi',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=c23',
    'warning_level=3',
    'werror=true',
  ],
)

xkbcommon = dependency('xkbcommon')
cairo = dependency('cairo')
pango = dependency('pango')
libgrapheme = dependency('libgrapheme')

wayland_client = dependency('wayland-client', version: '>=1.20')
wayland_protocols = dependency('wayland-protocols')
wayland_scanner = find_program('wayland-scanner')

wayland_protocols_dir = wayland_protocols.get_variable(
  pkgconfig: 'pkgdatadir',
)

session_lock_xml = join_paths(
  wayland_protocols_dir,
  'staging',
  'ext-session-lock',
  'ext-session-lock-v1.xml',
)

message('Session-lock protocol: ' + session_lock_xml)

session_lock_header = custom_target(
  'ext-session-lock-client-header',
  input: session_lock_xml,
  output: 'ext-session-lock-v1-client-protocol.h',
  command: [
    wayland_scanner,
    'client-header',
    '@INPUT@',
    '@OUTPUT@',
  ],
)

session_lock_code = custom_target(
  'ext-session-lock-private-code',
  input: session_lock_xml,
  output: 'ext-session-lock-v1-protocol.c',
  command: [
    wayland_scanner,
    'private-code',
    '@INPUT@',
    '@OUTPUT@',
  ],
)

anvi_inc = include_directories('include')

executable(
  'anvi',
  [
    'src/main.c',
    'src/session_setup.c',
    'src/keyboard.c',
    'src/output.c',
    'src/buffer.c',
    'src/log.c',
    'src/text_buffer.c',
    'src/render.c',
    session_lock_header,
    session_lock_code,
  ],
  include_directories: anvi_inc,
  dependencies: [
    wayland_client,
    xkbcommon,
    cairo,
    pango,
    libgrapheme,
  ]
)

subdir('tests')
