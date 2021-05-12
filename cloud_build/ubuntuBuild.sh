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
# 3: download and unpack source code of libs for build
# 4: build

#1
echo "deb  http://old-releases.ubuntu.com cosmic universe" | tee -a /etc/apt/sources.list
apt-get update
#2
DEBIAN_FRONTEND="noninteractive" apt-get install wget libtiff-dev unzip build-essential libjsoncpp-dev libjpeg8-dev libgdk-pixbuf2.0-dev libcairo2-dev libsqlite3-dev cmake libglib2.0-dev libxml2-dev libopenjp2-7-dev g++-8 libgtest-dev -y
#3
cp /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h /usr/include/glib-2.0/glibconfig.h
mkdir build
cd build
wget https://github.com/uclouvain/openjpeg/archive/v2.3.0.zip > /dev/null
unzip v2.3.0.zip  > /dev/null
wget https://boostorg.jfrog.io/artifactory/main/release/1.69.0/source/boost_1_69_0.tar.gz  > /dev/null
tar xvzf boost_1_69_0.tar.gz  > /dev/null
wget https://dicom.offis.de/download/dcmtk/dcmtk362/dcmtk-3.6.2.zip  > /dev/null
unzip dcmtk-3.6.2.zip  > /dev/null
wget https://github.com/open-source-parsers/jsoncpp/archive/0.10.7.zip
unzip 0.10.7.zip > /dev/null
wget https://github.com/openslide/openslide/releases/download/v3.4.1/openslide-3.4.1.tar.gz
tar xvzf openslide-3.4.1.tar.gz  > /dev/null
wget https://www.ijg.org/files/jpegsr9c.zip
unzip jpegsr9c.zip > /dev/null
#4
cmake -DSTATIC_BUILD=ON -DTESTS_BUILD=ON ..
make -j12
./gTests
