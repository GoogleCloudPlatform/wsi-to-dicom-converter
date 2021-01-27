# WSI to DICOM Converter

This repository contains a tool that converts whole slide images (WSIs) to DICOM. To read the underlying whole slide images (WSIs), this tool relies on [OpenSlide](https://openslide.org), which supports a variety of file formats.

## Quickstart

### Debian-based Linux
To download the latest release, on the Releases tab, download the installer for your operating system and then run it. For example, if you're running on a Debian-based system, download the `wsi2dcm_x.y.z.deb` file and then run:

```
sudo apt install ./wsi2dcm_x.y.z.deb
```
Note: if you get an error about missing shared libraries, run `sudo ldconfig` or make sure that `/usr/local/lib` is in your `LD_LIBRARY_PATH`.

You may also need to install the following packages and their dependencies with `sudo apt-get install`:
* `libtiff-dev`
* `libxml2-dev`
* `libcairo-dev`
* `gtk2-engines-pixbuf`

### MacOS

```
brew install wsi2dcm.rb
```


If an installer isn't available for your operating system, see [Compiling from source](#compiling-from-source).

After you have installed the WSI to DICOM converter, to convert a file, run the following command:

```
wsi2dcm --input <wsiFile> --outFolder <folder for generated files> --seriesDescription <text description>
```

Test data is freely available from [OpenSlide](http://openslide.cs.cmu.edu/download/openslide-testdata/).

To see all available options, run: `wsi2dcm --help`.

## Complete set of parameters

##### input
Input wsi file, supported by openslide.
##### outFolder
Folder to store dcm files
##### tileHeight
Tile height px.
##### tileWidth
Tile width px.
##### levels
Number of levels to generate, levels == 0 means number of levels will be read from wsi file.
##### downsamples
Size factor for each level  for each level, downsample is size factor for each level.

eg: if base level size is 100x100 and downsamples is (1, 2, 10) then
- level0 100x100
- level1 50x50
- level2 10x10
##### startOn
Level to start generation.
##### stopOn
Level to stop generation.
##### sparse
Use TILED_SPARSE frame organization, by default it's TILED_FULL http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.17.3.html
##### compression
Compression, supported compressions: jpeg, jpeg2000, raw.
##### seriesDescription
(0008,103E) [LO] SeriesDescription Dicom tag.
##### studyId
(0020,000D) [UI] StudyInstanceUID Dicom tag.
##### seriesId
(0020,000E) [UI] SeriesInstanceUID Dicom tag.
##### jsonFile
Dicom json file with additional tags https://www.dicomstandard.org/dicomweb/dicom-json-format/
##### batch
Maximum frames in one file, as limit is exceeded new files is started.

eg: 3 files will be generated if batch is 10 and 30 frames in level
##### threads
Threads to consume during execution.
##### debug
Print debug messages: dimensions of levels, size of frames.
##### dropFirstRowAndColumn
Drop first row and column of the source image in order to workaround bug https://github.com/openslide/openslide/issues/268

## Compiling from source

If you're using Ubuntu, run the following command to download the dependencies and build the tool:


```shell
sudo ./cloud_build/ubuntuBuild.sh
```

Otherwise, follow these steps:

1. Download the source from the Releases tab or check out the repo.
2. Make sure that you have the following dependencies installed:

  - g++ >=8
  - cmake >=3
  - boost >=1.69: https://www.boost.org/users/history/version_1_69_0.html
  - dcmtk source ==3.6.2: https://dicom.offis.de/download/dcmtk/dcmtk362/dcmtk-3.6.2.zip
  - openslide >=3.4.1
  - libjpeg >= 8
  - openjpeg >= 2.3.0
  - jsoncpp >= 1.8.0

3. Run the following commands:

```shell
mkdir build
cd build
cp -R %dcmtkDir% ./dcmtk-3.6.2
cmake ..
make -j%threads%
```
