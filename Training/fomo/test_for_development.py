"""
This file is mainly used for developing some functions and testing. 
It is a file that can be deleted after the development is complete. 
Of course, you can also keep this file to continue developing and testing some functions if needed.
"""

import cv2
import numpy as np
import argparse
import os
import random
import json


def is_image_file(filename):
    return any(filename.endswith(extension) for extension in ['.png', '.jpg', '.jpeg', '.PNG', '.JPG', '.JPEG'])


print(is_image_file("C:/Users/John/Downloads/archive.zip"))


image_path_list = [os.path.join("C:/Users/John/Desktop/cosmo/MLX90640-and-esp32-cam/Training/fomo/source_data") + '/' + x for x in os.listdir(os.path.join("C:/Users/John/Desktop/cosmo/MLX90640-and-esp32-cam/Training/fomo/source_data")) if is_image_file(x)]

split_num = int(len(image_path_list) * 0.8)
train_split_list = image_path_list[:split_num]
test_split_list = image_path_list[split_num:]

#train data transform
label_list = []
for i, image_path in enumerate(train_split_list):
    img_name = os.path.splitext(os.path.basename(image_path))[0]
    img = cv2.imread(image_path)
    print(img)
