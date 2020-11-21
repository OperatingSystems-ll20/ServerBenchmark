import numpy as np
import imageio
import sys
import scipy
from scipy import ndimage


def applySobel(imagePath):
    im = imageio.imread(imagePath)
    im = im.astype('int32')
    gray = lambda rgb : np.dot(rgb[... , :3] , [0.299 , 0.587, 0.114]) 
    im = gray(im)
    dx = ndimage.sobel(im, 1)  # horizontal derivative
    dy = ndimage.sobel(im, 0)  # vertical derivative
    mag = np.hypot(dx, dy)  # magnitude
    mag *= 255.0 / np.max(mag)  # normalize (Q&D)
    mag = mag.astype('uint8')
    return mag

def saveImg(image, destPath):
    imageio.imwrite(destPath, image)

if __name__ == "__main__":
    img = applySobel(sys.argv[1])
    saveImg(img, sys.argv[2])

