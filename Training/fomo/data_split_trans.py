import cv2
import numpy as np
import argparse
import os
import random
import json

"""
Return 
If the file extension is '.png', '.jpg', '.jpeg', '.PNG', '.JPG', '.JPEG', it will return true. Otherwise, it will return false.

Parameters
- filename (string): The file directory

Returns 
true or false
"""
def is_image_file(filename):
    return any(filename.endswith(extension) for extension in ['.png', '.jpg', '.jpeg', '.PNG', '.JPG', '.JPEG'])

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='data split and transform')
    parser.add_argument('--data-directory', type=str, required=True,
                    help='Where to read the data from')
    parser.add_argument('--out-directory', type=str, required=True,
                    help='Where to write the data')

    parser.add_argument('--image-size', type=int, required=True,
                    help='Number of image size')
    parser.add_argument('--image-format', choices=['RGB', 'Gray'], required=True,
                    help='Learning rate')

    args, unknown = parser.parse_known_args()


    # Get all image paths inside "args.data_directory".
    image_path_list = [os.path.join(args.data_directory) + '/' + x for x in os.listdir(os.path.join(args.data_directory)) if is_image_file(x)]
    random.shuffle(image_path_list)

    split_num = int(len(image_path_list) * 0.8)
    train_split_list = image_path_list[:split_num]
    test_split_list = image_path_list[split_num:]


    #train data transform
    label_list = []
    for i, image_path in enumerate(train_split_list):
        img_name = os.path.splitext(os.path.basename(image_path))[0]  # 取得 image 的名稱
        img = cv2.imread(image_path)

        if args.image_format == 'RGB':
            img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        elif args.image_format == 'Gray':
            img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)


        img = cv2.resize(img, (args.image_size, args.image_size), interpolation=cv2.INTER_LINEAR)
        img = img/255
        img = np.expand_dims(img, axis = 0).astype('float32')
        if args.image_format == 'Gray':
            img = np.expand_dims(img, axis = 3)
        
        if i == 0:
            train_data = img
        else:
            train_data = np.concatenate((train_data, img), axis=0)

        label_data = {'sampleId' : i, 'boundingBoxes' : []}
        with open(os.path.join(args.data_directory, img_name + '.txt')) as f:
            boundingBoxes = []
            for line in f.readlines():
                yolo_label = line.strip('\n').split(' ')
                w = int(float(yolo_label[3]) * args.image_size)
                h = int(float(yolo_label[4]) * args.image_size)
                x = int((float(yolo_label[1]) * args.image_size) - (w/2))
                y = int((float(yolo_label[2]) * args.image_size) - (h/2))
                boundingBoxes.append({'label' : int(yolo_label[0])+1, 'x':x, 'y':y, 'w':w, 'h':h})
        label_data['boundingBoxes'] = boundingBoxes
        label_list.append(label_data)


    np.save(os.path.join(args.out_directory, 'X_split_train.npy'), train_data)
    with open(os.path.join(args.out_directory, 'Y_split_train.npy'), "w") as f:
        json.dump(label_list, f)


    #test data transform
    label_list = []
    for i, image_path in enumerate(test_split_list):
        img_name = os.path.splitext(os.path.basename(image_path))[0]
        img = cv2.imread(image_path)

        if args.image_format == 'RGB':
            img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        elif args.image_format == 'Gray':
            img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)


        img = cv2.resize(img, (args.image_size, args.image_size), interpolation=cv2.INTER_LINEAR)
        img = img/255
        img = np.expand_dims(img, axis = 0).astype('float32')
        if args.image_format == 'Gray':
            img = np.expand_dims(img, axis = 3)

        if i == 0:
            test_data = img
        else:
            test_data = np.concatenate((test_data, img), axis=0)

        label_data = {'sampleId' : i, 'boundingBoxes' : []}
        with open(os.path.join(args.data_directory, img_name + '.txt')) as f:
            boundingBoxes = []
            for line in f.readlines():
                yolo_label = line.strip('\n').split(' ')
                w = int(float(yolo_label[3]) * args.image_size)
                h = int(float(yolo_label[4]) * args.image_size)
                x = int((float(yolo_label[1]) * args.image_size) - (w/2))
                y = int((float(yolo_label[2]) * args.image_size) - (h/2))
                boundingBoxes.append({'label' : int(yolo_label[0])+1, 'x':x, 'y':y, 'w':w, 'h':h})
        label_data['boundingBoxes'] = boundingBoxes
        label_list.append(label_data)


    np.save(os.path.join(args.out_directory, 'X_split_test.npy'), test_data)
    with open(os.path.join(args.out_directory, 'Y_split_test.npy'), "w") as f:
        json.dump(label_list, f)


