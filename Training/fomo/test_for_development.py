from resources.libraries.ei_shared.parse_train_input import parse_train_input, parse_input_shape
import os
input = parse_train_input("train_input.json")



BEST_MODEL_PATH = os.path.join(os.sep, 'tmp', 'best_model.tf' if input.akidaModel else 'best_model.hdf5')
print(input.inputShapeString)