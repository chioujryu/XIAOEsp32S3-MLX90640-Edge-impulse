#include "image_processing.h"

/* Includes ---------------------------------------------------------------- 
   這個你需要先在 Edge Impulse 訓練好模型，並下載成 Arduino 的 zip，才會有這個 Library*/
#include <XIAO_ESP32S3-CAM-Fruits-vs-Veggies-v2_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"

#include <stdint.h>

#include "buffer_frame.h"

#include "Arduino.h"


// ===將 RAW JPG 照片轉換成 RGB888===
uint8_t * RawJPGtoRGB888(camera_fb_t * fb){
  rgb888_buf_1 = (uint8_t *) heap_caps_malloc(fb->width * fb->height * CAMERA_FRAME_BYTE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t RGB888_buf_size = 0;
  
  if(rgb888_buf_1 == NULL)
  {
    printf("Malloc failed to allocate buffer for JPG.\n");
  }
  else
  {
    // Convert the RAW image into JPG
    // The parameter "31" is the JPG quality. Higher is better.
    fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf_1);
  }
  return rgb888_buf_1;
}


// ===將 RGB888 照片轉換成 JPG===
size_t RGB888toJPG(uint8_t * rgb888_buf_1, uint16_t height,uint16_t width){
  
  jpg_buf = (uint8_t *) heap_caps_malloc(height*width*3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t jpg_size = 0;
  
  if(jpg_buf == NULL){
      printf("Malloc failed to allocate buffer for JPG.\n");
  }else{
      // Convert the RAW image into JPG
      // The parameter "31" is the JPG quality. Higher is better.
      fmt2jpg(rgb888_buf_1, height*width*3, height, width, PIXFORMAT_RGB888, 31, &jpg_buf, &jpg_size);
      printf("Converted JPG size: %d bytes \n", jpg_size);
  }
  free(rgb888_buf_1);
  return jpg_size;
}

// ===Resize Image===
int resize_image(const uint8_t *srcImage,int srcWidth,int srcHeight,uint8_t *dstImage,int dstWidth,int dstHeight,int pixel_size_B){
                      
  ei::image::processing::resize_image(srcImage,
                                      srcWidth,
                                      srcHeight,
                                      dstImage,
                                      dstWidth,
                                      dstHeight,
                                      pixel_size_B);
  return pixel_size_B;
}


// Convert thermal to GRAYSCALE
uint8_t getGrayscaleValue(float temp, float minTemp, float maxTemp) {
  
  float ratio = (temp - minTemp) / (maxTemp - minTemp);  
  uint8_t value = ratio * 255;
  return value;
}


// Convert thermal pixel to R of RGB
uint8_t mapValueToColor_r(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  Serial.printf("\nratio = %f", ratio);
  
  float r;
  if (ratio >= 0.66) {
    r = 255;
  } else if (ratio >= 0.33) {
    r = 255;
  } else {
    r = 255 * ratio / 0.33;
  }
  uint8_t int_r = r;
  Serial.printf("\n int_r = %d \n", int_r);
  Serial.printf("\n sizeof int_r = %d \n", sizeof(int_r));
  return int_r;
}

// Convert thermal pixel to G of RGB
uint8_t mapValueToColor_g(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  Serial.printf("\nratio = %f", ratio);
  
  float g;
  if (ratio >= 0.66) {
    g = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    g = 255 * (ratio - 0.33) / 0.33;  // 修改此处使绿色显示较少
  } else {
    g = 255;
  }
  uint8_t int_g = g;
  Serial.printf("\nint_g = %d  \n",int_g);
  Serial.printf("\n sizeof int_g = %d  \n",sizeof(int_g));

  return int_g;
}

// Convert thermal pixel to B of B
uint8_t mapValueToColor_b(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  Serial.printf("\nratio = %f", ratio);
  
  float b;
  if (ratio >= 0.66) {
    b = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    b = 0;
  } else {
    b = 255 * (1 - ratio / 0.33);  // 修改此处使蓝色更深
  }
  uint8_t int_b = b;
  Serial.printf("\n int_b = %d  \n",int_b);
  Serial.printf("\n int_b = %d  \n", sizeof(int_b));

  return int_b;
}

// === convert thermal to RGB888 ===
void thermal_to_rgb888(){  //float mlx90640To_data[768], float minTemp, float maxTemp
  Serial.println("");
  Serial.printf("minTemp = %f", minTemp);
  Serial.printf("\nmaxTemp = %f", maxTemp);
  
  rgb888_buf_32x24 = (uint8_t *) heap_caps_malloc(32*24*3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  // 將 mlx90640To_data 的每個像素的溫度值 mapping 成 rgb，並存到 rgb888_photo
  for (int i = 0; i < 768; i++) {
    float temperature = mlx90640To[i];
    Serial.printf("temperature = %f", temperature);
    uint8_t r = mapValueToColor_r(temperature);
    uint8_t g = mapValueToColor_g(temperature);
    uint8_t b = mapValueToColor_b(temperature);
   
    rgb888_buf_32x24[i * 3] = b;   // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf_32x24[i * 3 + 1] = g;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf_32x24[i * 3 + 2] = r;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
  }
}
