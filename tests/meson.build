setup_test = executable(
  'test_setup',
  'test_setup.c',
)

test('setup', setup_test)

text_buffer_test = executable(
  'text_buffer_test',
  'text_buffer_test.c',
  '../src/text_buffer.c',
  '../src/log.c',
  include_directories: anvi_inc,
  dependencies: libgrapheme,
)

test('text buffer', text_buffer_test)
