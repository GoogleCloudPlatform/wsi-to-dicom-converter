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
# 1: update of ubuntu deb repos
# 2: install of tools and libs for build
# 3: install libjpeg turbo
# 4: install openjpeg
# 5: install opencv
# 6: install abseil
# 7: install dcmtk
# 8: install boost
# 9: install openslide
# 10: install jsoncpp
# 11: build

#1
echo "deb  http://old-releases.ubuntu.com cosmic universe" | tee -a /etc/apt/sources.list
apt-get update
#2
DEBIAN_FRONTEND="noninteractive" apt-get install wget libtiff-dev unzip build-essential libjsoncpp-dev libgdk-pixbuf2.0-dev libcairo2-dev libsqlite3-dev cmake libglib2.0-dev libxml2-dev libopenjp2-7-dev g++-9 libgtest-dev -y
#3
# installing in /workspace
echo "WorkingDIR"
pwd
apt-get install -y nasm
wget -O libjpeg_turbo.zip https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.4.zip
unzip libjpeg_turbo.zip
rm libjpeg_turbo.zip
mkdir -p ./libjpeg-turbo-2.1.4/build
cd ./libjpeg-turbo-2.1.4/build
cmake -G"Unix Makefiles" ..
make -j12
make install
cd ..
cd ..
rm -rf libjpeg-turbo-2.1.4
echo "WorkingDIR"
pwd
#5
apt-get install -y liblcms2-dev libzstd-dev libwebp-dev
wget -O v2.5.0.zip  https://github.com/uclouvain/openjpeg/archive/v2.5.0.zip > /dev/null
unzip v2.5.0.zip
mkdir -p ./openjpeg-2.5.0/build
cd ./openjpeg-2.5.0/build
cmake  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:bool=on  -DCMAKE_INSTALL_PREFIX:path="/usr" ..
make -j12
make install
make clean
cd ..
cd ..
rm -rf openjpeg-2.5.0
echo "WorkingDIR"
pwd
#4
wget -O opencv.zip https://github.com/opencv/opencv/archive/refs/tags/4.6.0.zip > /dev/null
unzip opencv.zip  > /dev/null
rm opencv.zip
mkdir -p ./opencv-4.6.0/build
cd ./opencv-4.6.0/build
cmake .. -DBUILD_LIST=core,imgproc,imgcodecs
make -j12
make install
cd ..
cd ..
rm -rf opencv-4.6.0
echo "WorkingDIR"
pwd
#6
wget -O abseil.zip https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.0.zip > /dev/null
unzip abseil.zip > /dev/null
rm abseil.zip
mkdir -p ./abseil-cpp-20220623.0/build
cd ./abseil-cpp-20220623.0/build
cmake ..  -DCMAKE_INSTALL_PREFIX=/abseil/install
cmake  --build . --target install
cd ..
cd ..
rm -rf abseil-cpp-20220623.0
echo "WorkingDIR"
pwd
#7
wget -O dcmtk-3.6.7.zip https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-3.6.7.zip > /dev/null
unzip dcmtk-3.6.7.zip > /dev/null
rm dcmtk-3.6.7.zip
mkdir -p ./dcmtk-DCMTK-3.6.7/build
cd ./dcmtk-DCMTK-3.6.7/build
cmake -DDCMTK_FORCE_FPIC_ON_UNIX:BOOL=TRUE -DDCMTK_ENABLE_CXX11:BOOL=TRUE -DDCMTK_ENABLE_CHARSET_CONVERSION:BOOL=FALSE -DBUILD_SHARED_LIBS:BOOL=ON ..
make -j12
make DESTDIR=/ install
cd ..
cd ..
rm -rf dcmtk-DCMTK-3.6.7
echo "WorkingDIR"
pwd
export DCMDICTPATH=/usr/local/share/dcmtk/dicom.dic
export PATH=/usr/local/bin:$PATH
# 8
wget -O boost_1_80_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_80_0.tar.gz
tar xvzf boost_1_80_0.tar.gz
rm boost_1_80_0.tar.gz
cd boost_1_80_0
./bootstrap.sh --prefix=/usr/ > /dev/null
./b2
./b2 install > /dev/null
cd ..
rm -rf boost_1_80_0
echo "WorkingDIR"
pwd
# 9
wget -O openslide-3.4.1.tar.gz https://github.com/openslide/openslide/releases/download/v3.4.1/openslide-3.4.1.tar.gz
tar xvzf openslide-3.4.1.tar.gz
rm openslide-3.4.1.tar.gz
cd openslide-3.4.1
apt-get install -y autoconf automake libtool pkg-config
autoreconf -i
./configure
make -j12
make install
cd ..
rm -rf openslide-3.4.1
apt-get purge -y autoconf
echo "WorkingDIR"
pwd
# Enable python to find openslide library
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
#10
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
echo "WorkingDIR"
pwd
#11
cp /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h /usr/include/glib-2.0/glibconfig.h
mkdir build
cd build
cmake -DSTATIC_BUILD=ON -DTESTS_BUILD=ON ..
make -j12
./gTests
