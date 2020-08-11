# Necessary dirs
mkdir -p local ext

# SDL2
if [ ! -d "ext/SDL" ]
then
	pushd ext
	git clone https://github.com/SDL-mirror/SDL
	popd
fi

# NEED TO WRITE BETTER THE PREFIX PATH
pushd ext/SDL
./configure --disable-esd --disable-arts \
            --disable-video-directfb --disable-rpath \
            --enable-alsa --enable-alsa-shared \
            --enable-pulseaudio --enable-pulseaudio-shared \
            --enable-x11-shared --enable-sdl-dlopen --disable-input-tslib \
            --prefix=/home/igor/Projects/hmh/local/  && \
make -j 4 && \
make install
popd


# Steam-Runtime
if [ ! -d "ext/steam-runtime" ]
then
	pushd ext
	git clone https://github.com/ValveSoftware/steam-runtime
	popd
fi

# Makes sure 
pip3 install python-debian
python3 ./build-runtime.py --output=$(pwd)/runtime

