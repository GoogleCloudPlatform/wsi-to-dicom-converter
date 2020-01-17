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

compare () {
  generatedTags=$1
  tags=$2

  difflines=$(diff $generatedTags $tags | grep -E ">")
  if [ ! -z "$difflines" ]
  then
      echo $difflines
      exit 1
  fi
}


#building binaries
bash ./cloud_build/ubuntuBuild.sh
#installing dcmtk tools
apt-get install wget dcmtk -y

fileName=./tests/CMU-1-Small-Region.svs

#test - generate jpeg2000 DICOM, read by dcmtk and check with expected tags
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test1 --levels 9 --startOn 8 --compression jpeg2000
dcm2json ./endToEnd/level-8-frames-0-1.dcm ./endToEnd/test1GeneratedTags.json
compare ./endToEnd/test1GeneratedTags.json ./endToEnd/test1ExpectedTags.json

#test - generate jpeg DICOM, read by dcmtk and check with expected tags
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test2 --stopOn 1
dcm2json ./endToEnd/level-0-frames-0-30.dcm ./endToEnd/test2GeneratedTags.json
compare ./endToEnd/test2GeneratedTags.json ./endToEnd/test2ExpectedTags.json

#test - generate DICOM without compression and compare image with expected one
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test3  --levels 15 --startOn 14 --tileHeight 100  --tileWidth 100 --compression raw
dcm2pnm ./endToEnd/level-14-frames-0-1.dcm ./endToEnd/test3GeneratedImage.ppm
diff ./endToEnd/test3GeneratedImage.ppm ./endToEnd/test3ExpectedImage.ppm

#test - generate DICOM with dropped first row and column and compare image with expected one
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test4  --levels 15 --startOn 14 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn
dcm2pnm ./endToEnd/level-14-frames-0-1.dcm ./endToEnd/test4GeneratedImage.ppm
diff ./endToEnd/test4GeneratedImage.ppm ./endToEnd/test4ExpectedImage.ppm
