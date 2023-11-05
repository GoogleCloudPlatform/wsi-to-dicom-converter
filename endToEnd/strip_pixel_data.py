import sys

import pydicom

if __name__ == '__main__':
  ds = pydicom.dcmread(sys.argv[1])
  del ds['PixelData']
  ds.save_as(ds.filename)

