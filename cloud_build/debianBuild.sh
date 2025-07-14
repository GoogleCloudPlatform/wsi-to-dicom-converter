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

set -ex

#1
apt-get update 
apt-get upgrade -y
apt-get install -y --no-install-recommends \
    ca-certificates \
    apt-utils \
    wget \
    dpkg-dev \
    cmake \
    make \
    meson \
    ninja-build \
    g++ \
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
apt-get install -y --no-install-recommends nasm
wget -O libjpeg_turbo.zip https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.1.0.zip > /dev/null
unzip libjpeg_turbo.zip > /dev/null
rm libjpeg_turbo.zip
mkdir -p ./libjpeg-turbo-3.1.0/build
cd ./libjpeg-turbo-3.1.0/build
cmake -G"Unix Makefiles" ..
make -j12
make install
cd ..
cd ..
rm -rf libjpeg-turbo-3.1.0
#3
apt-get install -y --no-install-recommends liblcms2-dev libzstd-dev libwebp-dev
wget -O v2.5.3.zip  https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.5.3.zip > /dev/null
unzip v2.5.3.zip > /dev/null
mkdir -p ./openjpeg-2.5.3/build
cd ./openjpeg-2.5.3/build
cmake  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:bool=on  -DCMAKE_INSTALL_PREFIX:path="/usr" ..
make -j12
make install
make clean
cd ..
cd ..
rm -rf openjpeg-2.5.3
#4
wget -O opencv.zip https://github.com/opencv/opencv/archive/refs/tags/4.10.0.zip > /dev/null
unzip opencv.zip  > /dev/null
rm opencv.zip
mkdir -p ./opencv-4.10.0/build
cd ./opencv-4.10.0/build
cmake .. -DBUILD_LIST=core,imgproc,imgcodecs
make -j12
make install
cd ..
cd ..
rm -rf opencv-4.10.0
#5
wget -O abseil.zip https://github.com/abseil/abseil-cpp/archive/refs/tags/20240722.0.zip > /dev/null
unzip abseil.zip > /dev/null
rm abseil.zip
mkdir -p ./abseil-cpp-20240722.0/build
cd ./abseil-cpp-20240722.0/build
cmake ..
cmake  --build . --target install
cd ..
cd ..
rm -rf abseil-cpp-20240722.0
#6
wget -O dcmtk-3.6.9.zip https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-3.6.9.zip > /dev/null
unzip dcmtk-3.6.9.zip > /dev/null
rm dcmtk-3.6.9.zip
mkdir -p ./dcmtk-DCMTK-3.6.9/build
cd ./dcmtk-DCMTK-3.6.9/build
cmake -DDCMTK_FORCE_FPIC_ON_UNIX:BOOL=TRUE -DDCMTK_ENABLE_CXX11:BOOL=TRUE -DDCMTK_ENABLE_CHARSET_CONVERSION:BOOL=FALSE -DBUILD_SHARED_LIBS:BOOL=ON -DDCMTK_MODULES:STRING="oficonv;ofstd;oflog;dcmdata;dcmimgle;dcmimage;dcmjpeg;dcmjpls;dcmapps" ..
make -j12
make DESTDIR=/ install
cd ..
cd ..
rm -rf dcmtk-DCMTK-3.6.9
export DCMDICTPATH=/usr/local/share/dcmtk-3.6.9/dicom.dic
export PATH=/usr/local/bin:$PATH
# 7
wget -O boost_1_87_0.tar.gz https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz > /dev/null
tar xvzf boost_1_87_0.tar.gz > /dev/null
rm boost_1_87_0.tar.gz
cd boost_1_87_0
./bootstrap.sh --prefix=/usr/ --with-libraries=system,atomic,thread,chrono,program_options,log,filesystem > /dev/null
./b2
./b2 install > /dev/null
cd ..
rm -rf boost_1_87_0
# 8
wget -O openslide-4.0.0.tar.gz https://github.com/openslide/openslide/archive/refs/tags/v4.0.0.tar.gz > /dev/null
tar xvzf openslide-4.0.0.tar.gz > /dev/null
rm openslide-4.0.0.tar.gz
cd openslide-4.0.0
meson setup ./openslide-build
meson compile -C ./openslide-build
meson install -C ./openslide-build
cd ..
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:/usr/local/lib/aarch64-linux-gnu
#9
wget -O 1.9.6.zip https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip > /dev/null
unzip 1.9.6.zip  > /dev/null
mkdir -p ./jsoncpp-1.9.6/build > /dev/null
cd ./jsoncpp-1.9.6/build
cmake -DCMAKE_BUILD_TYPE=release -DBUILD_STATIC_LIBS=OFF -DBUILD_SHARED_LIBS=ON -G "Unix Makefiles" ..
make -j12
make install
cd ..
cd ..
rm -rf jsoncpp-1.9.6
#10
mkdir build
cd build
cmake -DSTATIC_BUILD=ON -DTESTS_BUILD=ON ..
set -e
make -j12
./gTests
set +e
