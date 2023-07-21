#pragma once

#include <stdint.h>

#include "esp_camera.h"


// **************** Constant defines ****************
#define CAMERA_RAW_FRAME_BUFFER_COLS           320
#define CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define CAMERA_RESIZE_FRAME_BUFFER_COLS        160
#define CAMERA_RESIZE_FRAME_BUFFER_ROWS        120
#define CAMERA_THERMAL_FRAME_BUFFER_COLS       32
#define CAMERA_THERMAL_FRAME_BUFFER_ROWS       24
#define CAMERA_FRAME_BYTE_SIZE                 3


// =======Variable define=======
bool overlay = false;   // 是否要疊加照片

float minTemp = 100; // Thermal Buffer 的最小溫度
float maxTemp = 0;   // Thermal Buffer 的最大溫度

uint8_t * JPGtoRGB888(camera_fb_t * fb);
uint8_t * RGB888toJPG(uint8_t * jpg_buf);
int resize_image(const uint8_t *srcImage,int srcWidth,int srcHeight,uint8_t *dstImage,int dstWidth,int dstHeight,int pixel_size_B);
