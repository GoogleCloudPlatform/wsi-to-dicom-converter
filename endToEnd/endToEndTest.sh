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

#test - use nearest neighbor downsampling,  generate jpeg2000 DICOM, read by dcmtk and check with expected tags
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test1 --levels 9 --startOn 8 --compression jpeg2000
dcm2json ./endToEnd/level-8-frames-0-1.dcm ./endToEnd/test1GeneratedTags.json
compare ./endToEnd/test1GeneratedTags.json ./endToEnd/test1ExpectedTags.json

#test - use nearest neighbor downsampling, generate jpeg DICOM, read by dcmtk and check with expected tags
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test2 --stopOn 1
dcm2json ./endToEnd/level-0-frames-0-30.dcm ./endToEnd/test2GeneratedTags.json
compare ./endToEnd/test2GeneratedTags.json ./endToEnd/test2ExpectedTags.json

#test - use nearest neighbor downsampling, generate DICOM without compression and compare image with expected one
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test3  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test3GeneratedImage.ppm
diff ./endToEnd/test3GeneratedImage.ppm ./endToEnd/test3ExpectedImage.ppm

#test - use nearest neighbor downsampling, generate DICOM with dropped first row and column and compare image with expected one
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test4  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test4GeneratedImage.ppm
diff ./endToEnd/test4GeneratedImage.ppm ./endToEnd/test4ExpectedImage.ppm

#test - use bilinear downsampling to generate DICOM and compare with expected image
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test5  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --bilinearDownsampling
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test5GeneratedImage.ppm
diff ./endToEnd/test5GeneratedImage.ppm ./endToEnd/test5ExpectedImage.ppm
rm ./endToEnd/level-5-frames-0-1.dcm

#test - use bilinear downsampling to generate DICOM with dropped first row and column and compare image with expected one
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test6  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --bilinearDownsampling
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test6GeneratedImage.ppm
diff ./endToEnd/test6GeneratedImage.ppm ./endToEnd/test6ExpectedImage.ppm

#test - use progressiveDowsampling & bilinear downsampling to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test7  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --bilinearDownsampling --progressiveDownsample
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test7GeneratedImage.ppm
diff ./endToEnd/test7GeneratedImage.ppm ./endToEnd/test7ExpectedImage.ppm

#test - use progressiveDowsampling & nearest neighbor downsampling to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test8  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --progressiveDownsample
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test8GeneratedImage.ppm
diff ./endToEnd/test8GeneratedImage.ppm ./endToEnd/test8ExpectedImage.ppm

#test - use uniformPixelSpacing, nearest neighbor downsampling, & progressiveDownsample to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test9  --levels 16 --tileHeight 100  --tileWidth 100 --compression raw --progressiveDownsample --uniformPixelSpacing --stopDownsamplingAtSingleFrame
dcm2pnm +Fa +Sxf 1.0 ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test9GeneratedImage.ppm
dcm2json ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test9Generated.json
diff ./endToEnd/test9GeneratedImage.ppm.0.ppm ./endToEnd/test9ExpectedImage.ppm.0.ppm
diff ./endToEnd/test9GeneratedImage.ppm.1.ppm ./endToEnd/test9ExpectedImage.ppm.1.ppm
diff ./endToEnd/test9GeneratedImage.ppm.2.ppm ./endToEnd/test9ExpectedImage.ppm.2.ppm
diff ./endToEnd/test9GeneratedImage.ppm.3.ppm ./endToEnd/test9ExpectedImage.ppm.3.ppm
compare ./endToEnd/test9Generated.json ./endToEnd/test9Expected.json

#test - use uniformPixelSpacing, bilinearDownsampling downsampling, & progressiveDownsample to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test10  --levels 16 --tileHeight 100  --tileWidth 100 --compression raw --progressiveDownsample --uniformPixelSpacing --stopDownsamplingAtSingleFrame --bilinearDownsampling
dcm2pnm +Fa +Sxf 1.0 ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test10GeneratedImage.ppm
dcm2json ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test10Generated.json
diff ./endToEnd/test10GeneratedImage.ppm.0.ppm ./endToEnd/test10ExpectedImage.ppm.0.ppm
diff ./endToEnd/test10GeneratedImage.ppm.1.ppm ./endToEnd/test10ExpectedImage.ppm.1.ppm
diff ./endToEnd/test10GeneratedImage.ppm.2.ppm ./endToEnd/test10ExpectedImage.ppm.2.ppm
diff ./endToEnd/test10GeneratedImage.ppm.3.ppm ./endToEnd/test10ExpectedImage.ppm.3.ppm
compare ./endToEnd/test10Generated.json ./endToEnd/test10Expected.json

#test - use uniformPixelSpacing & nearest neighbor downsampling to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test11  --levels 16 --tileHeight 100  --tileWidth 100 --compression raw --uniformPixelSpacing --stopDownsamplingAtSingleFrame
dcm2pnm +Fa +Sxf 1.0 ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test11GeneratedImage.ppm
dcm2json ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test11Generated.json
diff ./endToEnd/test11GeneratedImage.ppm.0.ppm ./endToEnd/test11ExpectedImage.ppm.0.ppm
diff ./endToEnd/test11GeneratedImage.ppm.1.ppm ./endToEnd/test11ExpectedImage.ppm.1.ppm
diff ./endToEnd/test11GeneratedImage.ppm.2.ppm ./endToEnd/test11ExpectedImage.ppm.2.ppm
diff ./endToEnd/test11GeneratedImage.ppm.3.ppm ./endToEnd/test11ExpectedImage.ppm.3.ppm
compare ./endToEnd/test11Generated.json ./endToEnd/test11Expected.json

#test - use uniformPixelSpacing & bilinearDownsampling downsampling to generate DICOM
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test12  --levels 16 --tileHeight 100  --tileWidth 100 --compression raw --uniformPixelSpacing --stopDownsamplingAtSingleFrame --bilinearDownsampling
dcm2pnm +Fa +Sxf 1.0  ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test12GeneratedImage.ppm
dcm2json ./endToEnd/level-4-frames-0-4.dcm ./endToEnd/test12Generated.json
diff ./endToEnd/test12GeneratedImage.ppm.0.ppm ./endToEnd/test12ExpectedImage.ppm.0.ppm
diff ./endToEnd/test12GeneratedImage.ppm.1.ppm ./endToEnd/test12ExpectedImage.ppm.1.ppm
diff ./endToEnd/test12GeneratedImage.ppm.2.ppm ./endToEnd/test12ExpectedImage.ppm.2.ppm
diff ./endToEnd/test12GeneratedImage.ppm.3.ppm ./endToEnd/test12ExpectedImage.ppm.3.ppm
compare ./endToEnd/test12Generated.json ./endToEnd/test12Expected.json

#test - jpg compression quality 50
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test13  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --compressionQuality 50
dcmj2pnm +cl ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test13GeneratedImage.ppm
diff ./endToEnd/test13GeneratedImage.ppm ./endToEnd/test13ExpectedImage.ppm

#test - jpg compression quality 95
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test14  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --compressionQuality 95
dcmj2pnm +cl ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test14GeneratedImage.ppm
diff ./endToEnd/test14GeneratedImage.ppm ./endToEnd/test14ExpectedImage.ppm