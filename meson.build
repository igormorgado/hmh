project('sdl_game', 'c',
          version: '0.1.0',
    meson_version: '>= 0.50.0',
  default_options: [ 'warning_level=0',
                     'c_std=gnu11',
		     'b_lundef=false',
                   ],
)

add_global_arguments(['-DGAME_INTERNAL'], language: 'c')

incdir = include_directories('include')

subdir('src')

