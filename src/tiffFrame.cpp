// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <arpa/inet.h>
#include <boost/log/trivial.hpp>
#include <jpeglib.h>

#include <string>
#include <utility>

#include "src/jpegUtil.h"
#include "src/tiffDirectory.h"
#include "src/tiffFrame.h"


namespace wsiToDicomConverter {

class TiffFrameJpgBytes {
 public:
  explicit TiffFrameJpgBytes(TiffFrame * framePtr);
  virtual ~TiffFrameJpgBytes();
  bool hasJpegTable() const;
  TiffDirectory *tiffDirectory() const;
  uint8_t *compressedJpeg() const;
  uint64_t compressedJpegSize() const;
  int64_t width() const;
  int64_t height() const;

  void getJpegMemory(std::unique_ptr<uint8_t []>*data, uint64_t *size);

 private:
  std::unique_ptr<TiffTile> tile_;
  TiffFrame* framePtr_;
  uint64_t constructJpegMemSize_;
  std::unique_ptr<uint8_t[]> constructJpegMem_;
  uint8_t *data_;
  uint64_t rawBufferSize_;
  int64_t tileWidth_, tileHeight_;
  std::unique_ptr<TiffTile> getTiffTileData(int64_t *tileWidth,
                                            int64_t *tileHeight);
  void constructJpeg(TiffTile *tile);
};

TiffFrameJpgBytes::TiffFrameJpgBytes(TiffFrame * framePtr) {
  framePtr_ = framePtr;
  constructJpegMem_ = nullptr;
  constructJpegMemSize_ = 0;
  tile_ = std::move(getTiffTileData(&tileWidth_, &tileHeight_));
  if (hasJpegTable()) {
      constructJpeg(tile_.get());
      data_ = constructJpegMem_.get();
      rawBufferSize_ = constructJpegMemSize_;
  } else  {
    data_ = tile_->rawBuffer_.get();
    rawBufferSize_ = tile_->rawBufferSize_;
  }
}

void TiffFrameJpgBytes::getJpegMemory(std::unique_ptr<uint8_t []>*data,
                                        uint64_t *size) {
  if (constructJpegMem_ != nullptr) {
    *data = std::move(constructJpegMem_);
  } else {
    *data = std::move(tile_->rawBuffer_);
  }
  *size = rawBufferSize_;
}

TiffFrameJpgBytes::~TiffFrameJpgBytes() {}

uint8_t *TiffFrameJpgBytes::compressedJpeg() const {
  return data_;
}

uint64_t TiffFrameJpgBytes::compressedJpegSize() const {
  return rawBufferSize_;
}

int64_t TiffFrameJpgBytes::width() const {
  return tileWidth_;
}
int64_t TiffFrameJpgBytes::height() const {
    return tileHeight_;
}

std::unique_ptr<TiffTile> TiffFrameJpgBytes::getTiffTileData(int64_t *tileWidth,
                                                     int64_t *tileHeight) {
  const TiffDirectory *dir = tiffDirectory();
  const uint64_t tileIndex = ((framePtr_->getLocationY() / dir->tileHeight()) *
                              dir->tilesPerRow()) +
                              (framePtr_->getLocationX() /  dir->tileWidth());
  *tileWidth = dir->tileWidth();
  *tileHeight = dir->tileHeight();
  return std::move(framePtr_->tiffFile()->tile(framePtr_->tiffFileLevel(), tileIndex));
}

void setShortBigEndian(uint8_t* APPO, int firstbyte, uint16_t val) {
  val = htons(val);
  uint8_t* mem = reinterpret_cast<uint8_t*>(&val);
  APPO[firstbyte] = mem[0];
  APPO[firstbyte + 1] = mem[1];
}

void TiffFrameJpgBytes::constructJpeg(TiffTile *tile) {
  /*
    Tiff frames from files with missing complete jpeg tiles
    require reconstruction of jpeg payload.

    jpeg file format = jfif fileformat

    * Tiles Missing APP0 header.
    * tabels for all tiles written in jpegTables tag (shared across all tiles)
    * raw tile datasetream contains just encoded image values.

    * reconstruction requires
      * image start tag (2 bytes)
      * creating image header (APPO)
      * adding tables  <- table data embedded with start / end tags.
      * adding pixel data.  <- embedded with start / end tags.
      * image end tag (2 bytes)
  */
  const uint8_t *data = tile->rawBuffer_.get();
  const uint64_t rawBufferSize = tile->rawBufferSize_;
  const TiffDirectory *dir = tiffDirectory();
  const int64_t tableDatasize = dir->jpegTableDataSize();
  const uint8_t *tableData = dir->jpegTableData();
  uint8_t APPO[] = {
                  0xff, 0xd8,  // start of image marker
                  0xff, 0xe0,  // appo marker
                  0x00, 0x00,  // placeholder for length excluding appo marker
                  0x4A, 0x46, 0x49, 0x46, 0x00,  // "JFIF" in ascii with null
                  0x00,  // density unit; 0 = no unit
                  0x00, 0x00,  // image width place holder
                  0x00, 0x00,  // image height place holder
                  0x00,  // thumbnail pixel horizontal; no thumbnail
                  0x00};  // thumbnail pixel vertical; no thumbnail

  // exclude start of image and appo markers
  setShortBigEndian(APPO, 4, sizeof(APPO) -4);
  setShortBigEndian(APPO, 12, dir->tileWidth());
  setShortBigEndian(APPO, 14, dir->tileHeight());
  // -6 exclude start and end markers in table data, and start maker in raw buffer.
  constructJpegMemSize_ = tableDatasize + rawBufferSize - 6 + sizeof(APPO);

  constructJpegMem_ = std::make_unique<uint8_t[]>(constructJpegMemSize_);
  memcpy(constructJpegMem_.get(), APPO, sizeof(APPO));
  memcpy(&(constructJpegMem_[sizeof(APPO)]), &(tableData[2]), tableDatasize - 4);
  memcpy(&(constructJpegMem_[sizeof(APPO) + tableDatasize - 4]), &(data[2]), rawBufferSize -2);
}

TiffDirectory * TiffFrameJpgBytes::tiffDirectory() const {
  return framePtr_->tiffDirectory();
}

bool TiffFrameJpgBytes::hasJpegTable() const {
  return tiffDirectory()->hasJpegTableData();
}

TiffFrame::TiffFrame(
    TiffFile *tiffFile, int64_t locationX, int64_t locationY, int64_t level,
    int64_t frameWidth, int64_t frameHeight):
    Frame(locationX, locationY, frameWidth, frameHeight, NONE, -1, true) {
  tiffFile_ = tiffFile;
  level_ = level;
}

TiffFile *TiffFrame::tiffFile() const {
  return tiffFile_;
}

int64_t TiffFrame::tiffFileLevel() const {
  return level_;
}

TiffDirectory * TiffFrame::tiffDirectory () const {
  return tiffFile()->directory(tiffFileLevel());
}

TiffFrame::~TiffFrame() {}

const J_COLOR_SPACE TiffFrame::jpegDecodeColorSpace () const {
  return tiffDirectory()->isPhotoMetricRGB() ? JCS_RGB : JCS_YCbCr;
}

bool TiffFrame::canDecodeJpeg() {
  if (done_) {
    return true;
  }
  const TiffFrameJpgBytes jpegBytes(this);
  const bool result = jpegUtil::canDecodeJpeg(jpegBytes.width(),
                                              jpegBytes.height(),
                                              jpegDecodeColorSpace(),
                                              jpegBytes.compressedJpeg(),
                                              jpegBytes.compressedJpegSize());
  if (result) {
    BOOST_LOG_TRIVIAL(info) << "Decoded jpeg in TIFF file.";
    return true;
  }
  BOOST_LOG_TRIVIAL(error) << "Error occured decoding jpeg in TIFF file.";
  return false;
}

std::string TiffFrame::getPhotoMetrInt() const {
  if (tiffDirectory()->isPhotoMetricRGB()) {
      return "RGB";
  } else {
      return "YBR_FULL_422";
  }
}

bool TiffFrame::has_compressed_raw_bytes() const {
  return true;
  //return data_ != nullptr;
}

void TiffFrame::clearRawABGRMem() {
  // Clear out DICOM memory.
  // DICOM memory and compressed memory are same thing.
  // For TiffFrames.  Data in native form is already compressed.
  // Compressed memory is cleared after dicom memory.
  data_ = nullptr;
}

void TiffFrame::incSourceFrameReadCounter() {
  // Reads from Tiff no source frame counter to increment.
}

void TiffFrame::clear_dicom_mem() {
  // DICOM memory and compressed memory are same thing.
  // For TiffFrames.  Data in native form is already compressed.
  // Compressed memory is cleared after dicom memory.
  data_ = nullptr;
}

int64_t TiffFrame::rawABGRFrameBytes(uint8_t *raw_memory,
                                      int64_t memorysize) {
  // For tiff frame data in native format is jpeg encoded.
  // to work with just uncompress and return.
  TiffFrameJpgBytes jpegBytes(this);
  std::unique_ptr<uint8_t[]> data;
  size_t size;
  jpegBytes.getJpegMemory(&data, &size);

  uint64_t abgrBufferSizeRead;
  jpegUtil::decodedJpeg(get_frame_width(), get_frame_height(),
                        jpegDecodeColorSpace(), data.get(), size,
                        &abgrBufferSizeRead, raw_memory, memorysize);
  //dec_read_counter(); unessary no memory to clear.
  return abgrBufferSizeRead;
}

void TiffFrame::sliceFrame() {
  TiffFrameJpgBytes jpegBytes(this);
  jpegBytes.getJpegMemory(&data_, &size_);
  BOOST_LOG_TRIVIAL(debug) << " Tiff extracted frame size: " << size_ /
                                                                1024 << "kb";
  done_ = true;
}

}  // namespace wsiToDicomConverter


