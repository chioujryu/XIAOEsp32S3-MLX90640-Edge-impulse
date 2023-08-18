// 找出陣列中的最大溫度跟最小溫度
void find_min_max_average_temp(float * buf, uint16_t width, uint16_t height, float * min_and_max_and_average){

  float minTemp = 100;
  float maxTemp = 0;
  float aveTemp = 0;

  // 找出圖像的最小溫度跟最大溫度
  for (int i = 0; i < width * height; i++) {  
    float temp = buf[i];  
    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp; 
    aveTemp += buf[i];
  }
  aveTemp = aveTemp / (width * height);

  min_and_max_and_average[0] = minTemp;
  min_and_max_and_average[1] = maxTemp;
  min_and_max_and_average[2] = aveTemp;
}

// 將單一 pixel 的溫度值，轉變成 R
uint8_t mapValueToColor_r(float temp, float * min_and_max_temp) {  // , float minTemp, float maxTemp  
  float ratio = (temp - min_and_max_temp[0]) / (min_and_max_temp[1] - min_and_max_temp[0]); 
  
  float r;
  if (ratio >= 0.66) {
    r = 255;
  } else if (ratio >= 0.33) {
    r = 255;
  } else {
    r = 255 * ratio / 0.33;
  }
  uint8_t int_r = r;
  return int_r;
}

// 將單一 pixel 的溫度值，轉變成 G
uint8_t mapValueToColor_g(float temp, float * min_and_max_temp) {
  float ratio = (temp - min_and_max_temp[0]) / (min_and_max_temp[1] - min_and_max_temp[0]);
  
  float g;
  if (ratio >= 0.66) {
    g = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    g = 255 * (ratio - 0.33) / 0.33;  // 修改此处使绿色显示较少
  } else {
    g = 255;
  }
  uint8_t int_g = g;

  return int_g;
}

// 將單一 pixel 的溫度值，轉變成 B
uint8_t mapValueToColor_b(float temp, float * min_and_max_temp) {
  float ratio = (temp - min_and_max_temp[0]) / (min_and_max_temp[1] - min_and_max_temp[0]);
  
  float b;
  if (ratio >= 0.66) {
    b = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    b = 0;
  } else {
    b = 255 * (1 - ratio / 0.33);  // 修改此处使蓝色更深
  }
  uint8_t int_b = b;

  return int_b;
}

// 將溫度的 pixel 轉換成 RGB888
void MLX90640_thermal_to_rgb888(float * thermal_buf, uint8_t * rgb888_buf, float * min_and_max_temp){  

  for (int i = 0; i < 768; i++) {
    float temperature = thermal_buf[i];
    uint8_t r = mapValueToColor_r(temperature, min_and_max_temp);
    uint8_t g = mapValueToColor_g(temperature, min_and_max_temp);
    uint8_t b = mapValueToColor_b(temperature, min_and_max_temp);
   
    rgb888_buf[i * 3] = b;  
    rgb888_buf[i * 3 + 1] = g;  
    rgb888_buf[i * 3 + 2] = r; 
  }
}



void find_min_max_average_thermal_in_bounding_box(float * thermal_buf, 
                                                  uint16_t thermal_buf_width, 
                                                  uint16_t thermal_buf_height,
                                                  uint16_t bounding_box_x, 
                                                  uint16_t bounding_box_y, 
                                                  uint16_t bounding_box_width, 
                                                  uint16_t bounding_box_height,
                                                  float * min_max_ave_thermal_in_bd_box)
{
  /*
  * @brief 在 bounding box 中找到溫度的最大, 最小, 平均值
  *
  * @param thermal_buf 溫度的 buffer, 如果是用MLX90640, 那 buffer 的寬跟高會是 32 x 24
  * @param thermal_buf_width 這是 buffer 的寬度
  * @param thermal_buf_height 這是 buffer 的高度
  * @param bounding_box_x 這是 bounding box x (左上角)
  * @param bounding_box_y 這是 bounding box y (右上角)
  * @param bounding_box_width 這是 bounding box 的寬度
  * @param bounding_box_height 這是 bounding box 高度
  * @param min_max_ave_thermal_in_bd_box 這是你最後要取得的最大, 最小, 平均值
  */
  if (bounding_box_x > thermal_buf_width || bounding_box_x < 0 || bounding_box_y > thermal_buf_height || bounding_box_y < 0){
    Serial.println("center of bounding is wrong, the center need to in 32x24");
    return;
  }

  // 獲取 bounding box 左上角的座標
  uint16_t bd_box_topLeft_x = bounding_box_x;
  uint16_t bd_box_topLeft_y = bounding_box_y;
  uint16_t bd_box_topLeft_index = find_image_buffer_index(thermal_buf_width, bd_box_topLeft_x, bd_box_topLeft_y, 1); // 獲取 bounding box 左上角的座標，在 Buffer 的 index
  //Serial.println("bd_box_topLeft_index: " + String(bd_box_topLeft_index));
  
  float minTemp = 100;
  float maxTemp = 0;
  float aveTemp = 0;

  // 將 bounding box 裡面的溫度值取出來
  for (size_t i = 0; i < bounding_box_height; i++){
    for (size_t k = 0; k < bounding_box_width; k++){
      float temp = thermal_buf[bd_box_topLeft_index + (thermal_buf_width * i) + k];
      if (temp < minTemp) minTemp = temp;
      if (temp > maxTemp) maxTemp = temp; 
      aveTemp += temp;
    }
  }
  aveTemp = aveTemp / (bounding_box_width * bounding_box_height);

  min_max_ave_thermal_in_bd_box[0] = minTemp;
  min_max_ave_thermal_in_bd_box[1] = maxTemp;
  min_max_ave_thermal_in_bd_box[2] = aveTemp;
}
