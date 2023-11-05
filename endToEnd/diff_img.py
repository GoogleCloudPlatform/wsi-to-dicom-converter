import sys

import PIL.Image
import numpy as np

if __name__ == '__main__':
  try:
    first_img = sys.argv[1]
    second_img = sys.argv[2]
    first = np.asarray(PIL.Image.open(first_img))
    second = np.asarray(PIL.Image.open(second_img))
    val = np.max(np.abs(first - second))
    if val > 3:
      print(f'{first_img} and {second_img} are different {val}.')
      sys.exit(1)
    sys.exit(0)
  except Exception as exp:
    print(exp)
    sys.exit(1)
