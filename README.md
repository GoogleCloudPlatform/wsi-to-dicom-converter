# WSI to DICOM Converter

This repository contains a tool that converts whole slide images (WSIs) to DICOM. To read the underlying whole slide images (WSIs), this tool relies on [OpenSlide](https://openslide.org), which supports a variety of file formats.

## Quickstart

### Debian-based Linux
To download the latest release, on the Releases tab, download the installer for your operating system and then run it. For example, if you're running on a Debian-based system, download the `wsi_x.y.z.deb` file and then run:

```
sudo apt install ./wsi_x.y.z.deb
```
Note: if you get an error about missing shared libraries, run `sudo ldconfig` or make sure that `/usr/local/lib` is in your `LD_LIBRARY_PATH`.

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
