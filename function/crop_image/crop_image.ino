/* Includes ---------------------------------------------------------------- */
#include <XIAO-ESP32-Person-and-bottle-detection_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"
#include <string>
#include "Esp.h"

#include "Arduino.h"


uint8_t * rgb888_buf_320x240 = NULL;
uint8_t * rgb888_buf_640x480 = NULL;

camera_fb_t * fb = NULL;

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21


void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}

void capture(){
  

  uint8_t * jpg_buf = NULL;
  size_t jpg_size = 0;

  // 清空照片的 buffer
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors

  // 拍照
  fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
  if (!fb) 
  {
    Serial.println("Camera capture failed");
  }

  
  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf_320x240);
  ei::image::processing::resize_image(rgb888_buf_320x240, 320, 240, rgb888_buf_640x480, 640, 480, 3);
  ei::image::processing::crop_image_rgb888_packed(rgb888_buf_640x480, 640, 480, 160, 120, rgb888_buf_320x240, 320, 240);
  fmt2jpg(rgb888_buf_320x240, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf, &jpg_size);
  

  esp_camera_fb_return(fb); 
  fb = NULL;
  free(jpg_buf);
  jpg_buf = NULL;
  jpg_size = 0;
}




void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("setup");
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  configInitCamera();
}

void loop() {
  Serial.println("loop begin");
  
  rgb888_buf_320x240 = (uint8_t *)malloc(320*240*3);
  rgb888_buf_640x480 = (uint8_t *)malloc(640*480*3);  //heap_caps_malloc(640*480*3, MALLOC_CAP_SPIRAM)
  
  logMemory();

  capture();

  free(rgb888_buf_320x240);
  free(rgb888_buf_640x480);
  rgb888_buf_320x240 = NULL;
  rgb888_buf_640x480 = NULL;
  
  logMemory();
  
  delay(10000);
}

void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; 
  config.frame_size = FRAMESIZE_QVGA;  // FRAMESIZE_QQVGA = 160x120 | FRAMESIZE_QVGA = 320x240 | FRAMESIZE_VGA = 640x480
  config.pixel_format = PIXFORMAT_JPEG; // for streaming  PIXFORMAT_GRAYSCALE, PIXFORMAT_JPEG
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;   // CAMERA_GRAB_LATEST （The only solution I have found to avoid that lag/delay with the buffered data is to add:）, CAMERA_GRAB_WHEN_EMPTY
  config.fb_location = CAMERA_FB_IN_PSRAM; 
  config.jpeg_quality = 5;   // 越低相片品質愈好
  config.fb_count = 1;
  
  // ===Initialize the Camera===
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x \n", err);
    return;
    //ESP.restart();
  }


  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2   設置飽和度
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 1);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable  
}
