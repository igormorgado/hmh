project('sdl_game', 'c',
          version: '0.1.0',
    meson_version: '>= 0.50.0',
  default_options: [ 'warning_level=3',
                     'c_std=gnu11',
                   ],
)

# add_global_arguments(['-Wl,-rpath,\'$ORIGIN\''], language: 'c')
add_global_arguments(['-Wno-unused-function'], language: 'c')
add_global_arguments(['-DDEBUG'], language: 'c')

incdir = include_directories('include')

subdir('src')

