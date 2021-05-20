bin_srcs = [ 'sdl_game.c' ]
game_srcs = [ 'game.c' ]

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: true)
dl_dep = cc.find_library('dl', required: true)

bin_deps = [ dependency('sdl2'), m_dep, dl_dep ]
game_deps = [ m_dep ]

shared_library('game',
	       game_srcs,
	       dependencies: game_deps,
	       include_directories: incdir)

executable('sdl_game',
	   bin_srcs,
	   dependencies: bin_deps,
	   include_directories: incdir,
	   build_rpath: '$ORIGIN:/root/hmh/local/lib')
