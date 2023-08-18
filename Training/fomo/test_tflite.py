import cv2
import os
import numpy as np
import argparse
import tensorflow as tf
import time

def is_image_file(filename):
    return any(filename.endswith(extension) for extension in ['.png', '.jpg', '.jpeg', '.PNG', '.JPG', '.JPEG'])

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='testing model')
    parser.add_argument('--data-directory', type=str, required=True,
                    help='Where to read the data from')
    parser.add_argument('--model-file', type=str, required=True,
                    help='Where to read the model from')

    parser.add_argument('--image-size', type=int, required=True,
                    help='Number of image size')
    parser.add_argument('--image-format', choices=['RGB', 'Gray'], required=True,
                    help='Learning rate')
    parser.add_argument('--output-thres', required=False, default=0.5,
                    help='output thres')

    args, unknown = parser.parse_known_args()

    image_path_list = [os.path.join(args.data_directory) + '/' + x for x in os.listdir(os.path.join(args.data_directory)) if is_image_file(x)]

    interpreter = tf.lite.Interpreter(model_path=args.model_file)
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    input_scale, input_zero_point = interpreter.get_input_details()[0]['quantization']
    output_scale, ouput_zero_point = interpreter.get_output_details()[0]['quantization']
    input_type = interpreter.get_input_details()[0]['dtype']
    output_wh = interpreter.get_output_details()[0]['shape'][1]

    if interpreter.get_input_details()[0]['quantization_parameters']['scales'].size == 0:
        input_scale = 1
    if interpreter.get_input_details()[0]['quantization_parameters']['zero_points'].size == 0:
        input_zero_point = 0
    if interpreter.get_output_details()[0]['quantization_parameters']['scales'].size == 0:
        output_scale = 1
    if interpreter.get_output_details()[0]['quantization_parameters']['zero_points'].size == 0:
        output_zero_point = 0

    #print(input_details)
    #print(output_details)

    colors = [[0, 0, 255], [0, 255, 0] ,[255, 0, 0] ,[0, 255, 255] ,[255, 0, 255] ,[255, 255, 0]]

    for image_path in image_path_list:
        src_img = cv2.imread(image_path)
        input_h = src_img.shape[0]
        input_w = src_img.shape[1]

        if args.image_format == 'RGB':
            img_o = cv2.cvtColor(src_img, cv2.COLOR_BGR2RGB)
        elif args.image_format == 'Gray':
            img_o = cv2.cvtColor(src_img, cv2.COLOR_BGR2GRAY)


        img_o = cv2.resize(img_o, (args.image_size, args.image_size), interpolation=cv2.INTER_LINEAR)
        img = img_o/255
        img = np.expand_dims(img, axis = 0)
        if args.image_format == 'Gray':
            img = np.expand_dims(img, axis = 3)

        img = img / input_scale + input_zero_point
        interpreter.set_tensor(input_details[0]['index'], img.astype(input_type))

        start_time = time.time()
        interpreter.invoke()
        stop_time = time.time()
        print('infer time: ', stop_time-start_time)

        output_data = interpreter.get_tensor(output_details[0]['index']).astype(np.float32)
        output_data = (output_data - ouput_zero_point) * output_scale
        
        output_img = np.squeeze(np.where(output_data > float(args.output_thres), 1, 0) * 255).astype(np.uint8)
        for i in range(output_img.shape[2]-1):
            class_img = output_img[...,i+1]
            num_labels, labels, stats, centers = cv2.connectedComponentsWithStats(class_img, connectivity=8, ltype=cv2.CV_32S)
            for t in range(1, num_labels, 1):
                x, y, w, h, area = stats[t]
                src_x = int((x/output_wh)*input_w)
                src_y = int((y/output_wh)*input_h)
                src_w = int((w/output_wh)*input_w)
                src_h = int((h/output_wh)*input_h)

                cv2.rectangle(src_img, (src_x, src_y), (src_x + src_w, src_y + src_h), (colors[i][0], colors[i][1], colors[i][2]), 2)

        cv2.imshow('test', src_img)
        key = cv2.waitKey(0)
        if key == 27:
            break
            

