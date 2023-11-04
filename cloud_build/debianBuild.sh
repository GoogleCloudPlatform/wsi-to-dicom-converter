# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script updates environment and build wsi2dcm by steps:
# 1: install of tools and libs for build
# 2: install libjpeg turbo
# 3: install openjpeg
# 4: install opencv
# 5: install abseil
# 6: install dcmtk
# 7: install boost
# 8: install openslide
# 9: install jsoncpp
# 10: build

#1
apt-get update 
apt-get upgrade -y
apt-get install -y \
    apt-utils \
    wget \
    build-essential \
    cmake \
    unzip \
    libgtest-dev \
    libxml2-dev \
    libcairo2-dev \
    libtiff-dev \
    libgtk-3-dev \
    sqlite3 \
    libsqlite3-dev \
    valgrind \
    libjsoncpp-dev \
    libgdk-pixbuf2.0-dev \
    libglib2.0-dev      
#2
# installing in /workspace
apt-get install -y nasm
wget -O libjpeg_turbo.zip https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.0.zip > /dev/null
unzip libjpeg_turbo.zip > /dev/null
rm libjpeg_turbo.zip
mkdir -p ./libjpeg-turbo-3.0.0/build
cd ./libjpeg-turbo-3.0.0/build
cmake -G"Unix Makefiles" ..
make -j12
make install
cd ..
cd ..
rm -rf libjpeg-turbo-3.0.0
#3
apt-get install -y liblcms2-dev libzstd-dev libwebp-dev
wget -O v2.5.0.zip  https://github.com/uclouvain/openjpeg/archive/v2.5.0.zip > /dev/null
unzip v2.5.0.zip > /dev/null
mkdir -p ./openjpeg-2.5.0/build
cd ./openjpeg-2.5.0/build
cmake  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:bool=on  -DCMAKE_INSTALL_PREFIX:path="/usr" ..
make -j12
make install
make clean
cd ..
cd ..
rm -rf openjpeg-2.5.0
#4
wget -O opencv.zip https://github.com/opencv/opencv/archive/refs/tags/4.8.0.zip > /dev/null
unzip opencv.zip  > /dev/null
rm opencv.zip
mkdir -p ./opencv-4.8.0/build
cd ./opencv-4.8.0/build
cmake .. -DBUILD_LIST=core,imgproc,imgcodecs
make -j12
make install
cd ..
cd ..
rm -rf opencv-4.8.0
#5
wget -O abseil.zip https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.1.zip > /dev/null
unzip abseil.zip > /dev/null
rm abseil.zip
mkdir -p ./abseil-cpp-20230802.1/build
cd ./abseil-cpp-20230802.1/build
cmake ..  -DCMAKE_INSTALL_PREFIX=/abseil/install
cmake  --build . --target install
cd ..
cd ..
rm -rf abseil-cpp-20230802.1
#6
wget -O dcmtk-3.6.7.zip https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-3.6.7.zip > /dev/null
unzip dcmtk-3.6.7.zip > /dev/null
rm dcmtk-3.6.7.zip
mkdir -p ./dcmtk-DCMTK-3.6.7/build
cd ./dcmtk-DCMTK-3.6.7/build
cmake -DDCMTK_FORCE_FPIC_ON_UNIX:BOOL=TRUE -DDCMTK_ENABLE_CXX11:BOOL=TRUE -DDCMTK_ENABLE_CHARSET_CONVERSION:BOOL=FALSE -DBUILD_SHARED_LIBS:BOOL=ON -DDCMTK_MODULES:STRING="ofstd;oflog;dcmdata;dcmimgle;dcmimage;dcmjpeg" ..
make -j12
make DESTDIR=/ install
cd ..
cd ..
rm -rf dcmtk-DCMTK-3.6.7
export DCMDICTPATH=/usr/local/share/dcmtk/dicom.dic
export PATH=/usr/local/bin:$PATH
# 7
wget -O boost_1_83_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.gz > /dev/null
tar xvzf boost_1_83_0.tar.gz > /dev/null
rm boost_1_83_0.tar.gz
cd boost_1_83_0
./bootstrap.sh --prefix=/usr/ --with-libraries=system,atomic,thread,chrono,program_options,log,filesystem > /dev/null
./b2
./b2 install > /dev/null
cd ..
rm -rf boost_1_83_0
# 8
apt-get update
apt-get install -y python3-full pip libjpeg-dev ninja-build
pip3 install --break-system-packages meson
wget -O openslide-4.0.0.tar.gz https://github.com/openslide/openslide/archive/refs/tags/v4.0.0.tar.gz > /dev/null
tar xvzf openslide-4.0.0.tar.gz > /dev/null
rm openslide-4.0.0.tar.gz
cd openslide-4.0.0
meson setup ./openslide-build
meson compile -C ./openslide-build
meson install -C ./openslide-build
cd ..
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/usr/local/lib/aarch64-linux-gnu
#9
wget -O 1.9.5.zip https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.5.zip > /dev/null
unzip 1.9.5.zip  > /dev/null
mkdir -p ./jsoncpp-1.9.5/build > /dev/null
cd ./jsoncpp-1.9.5/build
cmake -DCMAKE_BUILD_TYPE=release -DBUILD_STATIC_LIBS=OFF -DBUILD_SHARED_LIBS=ON -G "Unix Makefiles" ..
make -j12
make install
cd ..
cd ..
rm -rf jsoncpp-1.9.5
#10
cp /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h /usr/include/glib-2.0/glibconfig.h
mkdir build
cd build
cmake -DSTATIC_BUILD=ON -DTESTS_BUILD=ON ..
make -j12
./gTests
