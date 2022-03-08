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

#set enviromnental vars for DCMTK
export DCMDICTPATH=/dcmtk/usr/local/share/dcmtk/dicom.dic
export PATH=/dcmtk/usr/local/bin:$PATH

fileName=./tests/CMU-1-Small-Region.svs
jpegFileName=./tests/bone.jpeg

#test - use nearest neighbor downsampling,  generate jpeg2000 DICOM, read by dcmtk and check with expected tags
echo "Test 1"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test1 --levels 9 --startOn 8 --compression jpeg2000
dcm2json ./endToEnd/level-8-frames-0-1.dcm ./endToEnd/test1GeneratedTags.json
compare ./endToEnd/test1GeneratedTags.json ./endToEnd/test1ExpectedTags.json

#test - use nearest neighbor downsampling, generate jpeg DICOM, read by dcmtk and check with expected tags
echo "Test 2"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test2 --stopOn 1
dcm2json ./endToEnd/level-0-frames-0-30.dcm ./endToEnd/test2GeneratedTags.json
compare ./endToEnd/test2GeneratedTags.json ./endToEnd/test2ExpectedTags.json

#test - use nearest neighbor downsampling, generate DICOM without compression and compare image with expected one
echo "Test 3"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test3  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test3GeneratedImage.ppm
diff ./endToEnd/test3GeneratedImage.ppm ./endToEnd/test3ExpectedImage.ppm

#test - use nearest neighbor downsampling, generate DICOM with dropped first row and column and compare image with expected one
echo "Test 4"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test4  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test4GeneratedImage.ppm
diff ./endToEnd/test4GeneratedImage.ppm ./endToEnd/test4ExpectedImage.ppm

#test - use bilinear downsampling to generate DICOM and compare with expected image
echo "Test 5"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test5  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --opencvDownsampling=AREA
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test5GeneratedImage.ppm
diff ./endToEnd/test5GeneratedImage.ppm ./endToEnd/test5ExpectedImage.ppm
rm ./endToEnd/level-5-frames-0-1.dcm

#test - use bilinear downsampling to generate DICOM with dropped first row and column and compare image with expected one
echo "Test 6"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test6  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --opencvDownsampling=LANCZOS4
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test6GeneratedImage.ppm
diff ./endToEnd/test6GeneratedImage.ppm ./endToEnd/test6ExpectedImage.ppm

#test - use progressiveDowsampling & bilinear downsampling to generate DICOM
echo "Test 7"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test7  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --opencvDownsampling=CUBIC --progressiveDownsample
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test7GeneratedImage.ppm
diff ./endToEnd/test7GeneratedImage.ppm ./endToEnd/test7ExpectedImage.ppm

#test - use progressiveDowsampling & nearest neighbor downsampling to generate DICOM
echo "Test 8"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test8  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --progressiveDownsample
dcm2pnm ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test8GeneratedImage.ppm
diff ./endToEnd/test8GeneratedImage.ppm ./endToEnd/test8ExpectedImage.ppm

#test - jpg compression quality 50
echo "Test 9"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test9  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 50
dcmj2pnm +cl ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test9GeneratedImage.ppm
diff ./endToEnd/test9GeneratedImage.ppm ./endToEnd/test9ExpectedImage.ppm

#test - jpg compression quality 95
echo "Test 10"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test10  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 95
dcmj2pnm +cl ./endToEnd/level-5-frames-0-1.dcm ./endToEnd/test10GeneratedImage.ppm
diff ./endToEnd/test10GeneratedImage.ppm ./endToEnd/test10ExpectedImage.ppm

#test - raw svs frame extraction.
echo "Test 11"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test11  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 95 --SVSImportPreferScannerTileingForLargestLevel --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame
dcmj2pnm +cl ./endToEnd/level-4-frames-0-1.dcm ./endToEnd/test11GeneratedImage.ppm
diff ./endToEnd/test11GeneratedImage.ppm ./endToEnd/test11ExpectedImage.ppm

#test - test create image pyrmaid from JPEG
echo "Test 12"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $jpegFileName ./endToEnd/ --seriesDescription test12  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readImage --untiledImageHeightMM 12.0
dcm2json ./endToEnd/level-2-frames-0-1.dcm ./endToEnd/test12GeneratedTags.json
compare ./endToEnd/test12GeneratedTags.json ./endToEnd/test12ExpectedTags.json

#test - test create image pyrmaid from RawDICOM
echo "Test 13"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/raw.dicom ./endToEnd/ --seriesDescription test13  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcm2json ./endToEnd/level-2-frames-0-1.dcm ./endToEnd/test13GeneratedTags.json
compare ./endToEnd/test13GeneratedTags.json ./endToEnd/test13ExpectedTags.json

#test - test create image pyrmaid from JpegDICOM 
echo "Test 14"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/jpeg.dicom ./endToEnd/ --seriesDescription test14  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcm2json ./endToEnd/level-2-frames-0-1.dcm ./endToEnd/test14GeneratedTags.json
compare ./endToEnd/test14GeneratedTags.json ./endToEnd/test14ExpectedTags.json

#test - test create image pyrmaid from Jpeg2000
echo "Test 15"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/jpeg2000.dicom ./endToEnd/ --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcm2json ./endToEnd/level-2-frames-0-1.dcm ./endToEnd/test15GeneratedTags.json
compare ./endToEnd/test15GeneratedTags.json ./endToEnd/test15ExpectedTags.json
