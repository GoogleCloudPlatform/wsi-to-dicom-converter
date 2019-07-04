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

version=${1:1}
path="cloud_build/rpm/BUILDROOT/wsi2dcm-$version-1.x86_64/"

#install rpmbuild into ubuntu
apt-get update
apt-get install rpm -y

#set rpmbuild config with current path
echo '%_topdir %(pwd)/cloud_build/rpm' >~/.rpmmacros
#create rpm package folder stucture
mkdir cloud_build/rpm/{BUILD,BUILDROOT,RPMS,SOURCES,SRPMS}
mkdir -p "$path"/usr/{bin,lib64}
cp ./build/wsi2dcm "$path"/usr/bin
cp ./build/libwsi2dcm.so "$path"/usr/lib64/
#set version
sed -i "s/Version:/Version: $version/g" cloud_build/rpm/SPECS/wsi2dcm.spec
#build rpm
rpmbuild -bb cloud_build/rpm/SPECS/wsi2dcm.spec
