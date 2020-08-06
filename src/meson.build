#ide:editable-filelist
sources = [ 'main.c' ]

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

deps = [ dependency('sdl2'), m_dep ]


executable('main', sources, dependencies: deps, include_directories: incdir)
