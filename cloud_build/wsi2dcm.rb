class Wsi2dcm < Formula
  desc "Conversion tool/library for converting whole slide images to DICOM "
  version ""
  homepage "https://github.com/GoogleCloudPlatform/wsi-to-dicom-converter"
  url ""
  sha256 ""

  depends_on "cmake" => :build
  depends_on "wget" => :build
  depends_on "unzip" => :build
  depends_on "gcc@9" => :build
  depends_on "openslide" => :build
  depends_on "openjpeg" => :build

  def install
    mkdir "wsi-build" do
      args = std_cmake_args
      args << "-Dpkg_config_libdir=#{lib} -DCMAKE_C_COMPILER=gcc-9 -DCMAKE_CXX_COMPILER=g++-9 " << ".."
      system "wget", "https://dicom.offis.de/download/dcmtk/dcmtk362/dcmtk-3.6.2.zip"
      system "unzip", "dcmtk-3.6.2.zip"	
      system "wget", "https://dl.bintray.com/boostorg/release/1.69.0/source/boost_1_69_0.tar.gz"
      system "tar", "xvzf", "boost_1_69_0.tar.gz"     
      system "wget", "https://github.com/open-source-parsers/jsoncpp/archive/0.10.7.zip"
      system "unzip", "0.10.7.zip"     
      system "cmake", *args
      system "make", "-j6"
      bin.install "wsi2dcm"
      lib.install "libwsi2dcm.dylib"
    end
  end
end
