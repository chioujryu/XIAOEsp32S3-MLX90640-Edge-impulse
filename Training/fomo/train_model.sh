python data_split_trans.py --data-directory source_data --out-directory train_data --image-size 96 --image-format Gray
python train.py --info-file train_input.json --data-directory train_data --out-directory output --epochs 30 --learning-rate 0.01
xxd -i output/model_quantized_int8_io.tflite > output/model_data.cc
