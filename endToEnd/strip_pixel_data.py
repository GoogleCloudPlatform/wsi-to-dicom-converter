import sys

import pydicom

if __name__ == '__main__':
  ds = pydicom.dcmread(sys.argv[1])
  try:
    del ds['PixelData']
  except KeyError:
    pass
  ds.file_meta.TransferSyntaxUID = '1.2.840.10008.1.2.1'
  ds.save_as(ds.filename)

