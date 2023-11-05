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
  set +e
  difflines=$(diff $generatedTags $tags | grep -E ">")
  set -e 
  if [ ! -z "$difflines" ]
  then
      echo ""
      echo "Diff in generated and expected JSON."
      echo $difflines
      exit 1
  fi
  return 0
}


if [ "$1" !=  "SKIP_BUILD_ENVIRONMENT" ]
then
  #building binaries
  bash ./cloud_build/debianBuild.sh
fi
pip install --break-system-packages pillow numpy pydicom
#set enviromnental vars for DCMTK
export DCMDICTPATH=/usr/local/share/dcmtk/dicom.dic
export PATH=/usr/local/bin:$PATH

fileName=./tests/CMU-1-Small-Region.svs
jpegFileName=./tests/bone.jpeg

set -e

#test - use nearest neighbor downsampling,  generate jpeg2000 DICOM, read by dcmtk and check with expected tags
echo "Test 1"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test1 --levels 9 --startOn 8 --compression jpeg2000 
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-256-frames-0-1.dcm
dcm2json ./endToEnd/downsample-256-frames-0-1.dcm ./endToEnd/test1GeneratedTags.json
compare ./endToEnd/test1GeneratedTags.json ./endToEnd/test1ExpectedTags.json

#test - use nearest neighbor downsampling, generate jpeg DICOM, read by dcmtk and check with expected tags
echo "Test 2"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test2 --stopOn 1
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-1-frames-0-30.dcm
dcm2json ./endToEnd/downsample-1-frames-0-30.dcm ./endToEnd/test2GeneratedTags.json
compare ./endToEnd/test2GeneratedTags.json ./endToEnd/test2ExpectedTags.json

#test - use nearest neighbor downsampling, generate DICOM without compression and compare image with expected one
echo "Test 3"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test3  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test3GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test3GeneratedImage.bmp ./endToEnd/test3ExpectedImage.bmp

#test - use nearest neighbor downsampling, generate DICOM with dropped first row and column and compare image with expected one
echo "Test 4"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test4  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test4GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test4GeneratedImage.bmp ./endToEnd/test4ExpectedImage.bmp

#test - use bilinear downsampling to generate DICOM and compare with expected image
echo "Test 5"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test5  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --opencvDownsampling=AREA
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test5GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test5GeneratedImage.bmp ./endToEnd/test5ExpectedImage.bmp

#test - use bilinear downsampling to generate DICOM with dropped first row and column and compare image with expected one
echo "Test 6"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test6  --levels 6 --startOn 5 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --opencvDownsampling=LANCZOS4
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test6GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test6GeneratedImage.bmp ./endToEnd/test6ExpectedImage.bmp

#test - use progressiveDowsampling & bilinear downsampling to generate DICOM
echo "Test 7"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test7  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --opencvDownsampling=CUBIC --progressiveDownsample
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test7GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test7GeneratedImage.bmp ./endToEnd/test7ExpectedImage.bmp

#test - use progressiveDowsampling & nearest neighbor downsampling to generate DICOM
echo "Test 8"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test8  --levels 6 --tileHeight 100  --tileWidth 100 --compression raw --dropFirstRowAndColumn --progressiveDownsample
dcm2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test8GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test8GeneratedImage.bmp ./endToEnd/test8ExpectedImage.bmp

#test - jpg compression quality 50
echo "Test 9"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test9  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 50
dcmj2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test9GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test9GeneratedImage.bmp ./endToEnd/test9ExpectedImage.bmp

#test - jpg compression quality 95
echo "Test 10"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test10  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 95
dcmj2pnm +obt ./endToEnd/downsample-32-frames-0-1.dcm ./endToEnd/test10GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test10GeneratedImage.bmp ./endToEnd/test10ExpectedImage.bmp

#test - raw svs frame extraction.
echo "Test 11"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test11  --levels 6 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 95 --SVSImportPreferScannerTileingForLargestLevel --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame
dcmj2pnm +obt ./endToEnd/downsample-16-frames-0-1.dcm ./endToEnd/test11GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test11GeneratedImage.bmp ./endToEnd/test11ExpectedImage.bmp

#test - test create image pyramid from JPEG
echo "Test 12"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $jpegFileName ./endToEnd/ --seriesDescription test12  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readImage --untiledImageHeightMM 12.0
dcmj2pnm +obt ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test12GeneratedImage.bmp
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-4-frames-0-1.dcm
dcm2json ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test12GeneratedTags.json
compare ./endToEnd/test12GeneratedTags.json ./endToEnd/test12ExpectedTags.json
python3 ./endToEnd/diff_img.py ./endToEnd/test12GeneratedImage.bmp ./endToEnd/test12ExpectedImage.bmp

#test - test create image pyramid from RawDICOM
echo "Test 13"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/raw.dicom ./endToEnd/ --seriesDescription test13  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcmj2pnm +obt ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test13GeneratedImage.bmp
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-4-frames-0-1.dcm
dcm2json ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test13GeneratedTags.json
compare ./endToEnd/test13GeneratedTags.json ./endToEnd/test13ExpectedTags.json
python3 ./endToEnd/diff_img.py ./endToEnd/test13GeneratedImage.bmp ./endToEnd/test13ExpectedImage.bmp

#test - test create image pyramid from JpegDICOM 
echo "Test 14"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/jpeg.dicom ./endToEnd/ --seriesDescription test14  --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcmj2pnm +obt ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test14GeneratedImage.bmp
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-4-frames-0-1.dcm
dcm2json ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test14GeneratedTags.json
compare ./endToEnd/test14GeneratedTags.json ./endToEnd/test14ExpectedTags.json
python3 ./endToEnd/diff_img.py ./endToEnd/test14GeneratedImage.bmp ./endToEnd/test14ExpectedImage.bmp

#test - test create image pyramid from Jpeg2000
echo "Test 15"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm ./tests/jpeg2000.dicom ./endToEnd/ --levels 6 --tileHeight 256  --tileWidth 256 --progressiveDownsample --compression=RAW --opencvDownsampling=CUBIC --stopDownsamplingAtSingleFrame --readDICOM
dcmj2pnm +obt ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test15GeneratedImage.bmp
python3 ./endToEnd/strip_pixel_data.py ./endToEnd/downsample-4-frames-0-1.dcm
dcm2json ./endToEnd/downsample-4-frames-0-1.dcm ./endToEnd/test15GeneratedTags.json
compare ./endToEnd/test15GeneratedTags.json ./endToEnd/test15ExpectedTags.json
python3 ./endToEnd/diff_img.py ./endToEnd/test15GeneratedImage.bmp ./endToEnd/test15ExpectedImage.bmp

#test - Selected magnfication generation, 1x & 13x
echo "Test 16"
rm ./endToEnd/*.dcm -f
./build/wsi2dcm $fileName ./endToEnd/ --seriesDescription test16 --downsamples 1 13 --tileHeight 100  --tileWidth 100 --progressiveDownsample --jpegCompressionQuality 95 --SVSImportPreferScannerTileingForLargestLevel --opencvDownsampling=AREA
dcmj2pnm +obt ./endToEnd/downsample-13-frames-0-1.dcm ./endToEnd/test16GeneratedImage.bmp
python3 ./endToEnd/diff_img.py ./endToEnd/test16GeneratedImage.bmp ./endToEnd/test16ExpectedImage.bmp
