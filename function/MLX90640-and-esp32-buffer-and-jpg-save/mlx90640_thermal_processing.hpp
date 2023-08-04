

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
  }

  // 找出圖像的平均溫度
  for (int i = 0; i < width * height; i++){
    aveTemp += buf[i];
  }
  aveTemp = aveTemp / (width * height);

  min_and_max_and_average[0] = minTemp;
  min_and_max_and_average[1] = maxTemp;
  min_and_max_and_average[2] = aveTemp;

  Serial.println("minTemp: " + String(min_and_max_and_average[0]) + "  " +
                 "maxTemp: " + String(min_and_max_and_average[1]) + "  " +
                 "aveTemp: " + String(min_and_max_and_average[2]));
}

// 將單一 pixel 的溫度值，轉變成 R
uint8_t mapValueToColor_r(float temp, float * min_and_max_temp) {  // , float minTemp, float maxTemp
  
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
