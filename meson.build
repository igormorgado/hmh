project('sdl_game', 'c',
          version: '0.1.0',
    meson_version: '>= 0.50.0',
  default_options: [ 'warning_level=2',
                     'c_std=gnu99',
                   ],
)

add_global_arguments(['-Wno-unused-function'], language: 'c')

incdir = include_directories('include')

subdir('src')

