#ide:editable-filelist
sources = [ 'sdl_game.c' ]

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: false)

deps = [ dependency('sdl2'), m_dep ]

# Add local installed libraries in  search path.. really nice ;-)
# TODO: Get this path more automaticly... 
executable('sdl_game', sources, dependencies: deps, include_directories:
  incdir, build_rpath: '$ORIGIN:/home/igor/Projects/hmh/local/lib')
