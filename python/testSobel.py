import numpy as np
import imageio
import sys
import scipy
from scipy import ndimage

def applySobel(imagePath, destPath):
    im = imageio.imread(imagePath)
    im = im.astype('int32')
    gray = lambda rgb : np.dot(rgb[... , :3] , [0.299 , 0.587, 0.114]) 
    im = gray(im)
    dx = ndimage.sobel(im, 1)  # horizontal derivative
    dy = ndimage.sobel(im, 0)  # vertical derivative
    mag = np.hypot(dx, dy)  # magnitude
    mag *= 255.0 / np.max(mag)  # normalize (Q&D)
    mag = mag.astype('uint8')
    imageio.imwrite(destPath, mag)


if __name__ == "__main__":
    applySobel(sys.argv[1], sys.argv[2])

