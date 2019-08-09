# WSI to DICOM Converter

This repository contains a tool that provides conversion of whole slide images (WSIs) to DICOM. This tool relies on [OpenSlide](https://openslide.org) to read the underlying whole slide images, which supports a variety of different file formats.

## Quickstart

The easiest way to get started is to check out the [Releases](/releases) tab to download the latest release. There you can download an installer for your operating system. For example, if you're running on a Debian based system, download the `wsi_x.y.z.deb` file and run:

```
sudo apt install ./wsi_x.y.z.deb
```

Note: if you get an error about missing shared libraries run `sudo ldconfig` or make sure `/usr/local/lib` is in your `LD_LIBRARY_PATH`.

If an installer isn't availble for you operating system, please see the instructions for compiling from source.

Then to convert a file you can run:

```
wsi2dcm --input <wsiFile> --outFolder <folder for generated files> --seriesDescription <text description>
```

Some test data is freely available from [OpenSlide](http://openslide.cs.cmu.edu/download/openslide-testdata/).

To see all available options run: `wsi2dcm --help`.

## Compiling from source

To get the source either download from the [Releases](/releases) tab or checkout the repo directly.

Dependencies:
  - g++ >=8
  - cmake >=3
  - boost >=1.69: https://www.boost.org/users/history/version_1_69_0.html
  - dcmtk source ==3.6.2: https://dicom.offis.de/download/dcmtk/dcmtk362/dcmtk-3.6.2.zip
  - openslide >=3.4.1
  - libjpeg >= 8
  - openjpeg >= 2.3.0
  - jsoncpp >= 1.8.0


If you're using Ubuntu then there is a script to help download dependencies and build the tool:

```shell
sudo ./cloud_build/ubuntuBuild.sh
```

Otherwise, make sure you've downloaded and installed the required dependencies and then run:

```shell
mkdir build
cd build
cp -R %dcmtkDir% ./dcmtk-3.6.2 
cmake ..
make -j%threads%
```
