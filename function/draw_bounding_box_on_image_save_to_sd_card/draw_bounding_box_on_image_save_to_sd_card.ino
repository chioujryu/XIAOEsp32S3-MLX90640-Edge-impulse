/* Includes ---------------------------------------------------------------- */

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


uint8_t * rgb888_buf_320x240_esp32 = (uint8_t *)malloc(320*240*3);;
size_t imageCount = 0;

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#if defined(CAMERA_MODEL_XIAO_ESP32S3)
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
#else
#error "Camera model not selected"
#endif


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("setup");

  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  // Initialize SD card
  if(!SD.begin(21)){
    Serial.println("Card Mount Failed");
    return;}
  // Determine if the type of SD card is available
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;}
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  // Calculate SD Size
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  //PSRAM Initialisation
  if(psramInit()){
          Serial.println("The PSRAM is correctly initialized");
  }else{
          Serial.println("PSRAM does not work");
  }

  configInitCamera();
}

void loop() {

  bool whether_to_check_memory_usage = false;

  if(Serial.available()>0){
    char receivedChar = Serial.read();
    if (receivedChar == '1'){
      Serial.println("Enter '1' in serial, capture a photo using esp32");
      char filename[32];
      sprintf(filename, "/%d_image.jpg", imageCount);
      capture(filename, rgb888_buf_320x240_esp32);
      whether_to_check_memory_usage = true;
      imageCount ++;
    }
  }
  if (whether_to_check_memory_usage == true){logMemory();}
  
  delay(100);
}

void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}

void capture(const char * file_dir, uint8_t * rgb888_buf_320x240){
  
  uint8_t * jpg_buf = NULL;
  size_t jpg_size = 0;
  camera_fb_t * fb = NULL;

  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors
  fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
  if (!fb) {
    Serial.println("Camera capture failed");
  }else{Serial.println("Camera capture one shot");}

  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf_320x240);


  uint16_t bounding_box_center_x = 25 ;
  uint16_t bounding_box_center_y = 25;
  uint16_t bounding_box_width = 50;
  uint16_t bounding_box_height = 50;
  uint16_t corrected_bb_x;
  uint16_t corrected_bb_y;
  uint16_t corrected_bb_width;
  uint16_t corrected_bb_height;

  // 假設原始的照片是 96x96, 校正 bounding box
  bounding_box_correction(  96, 
                            96, 
                            bounding_box_center_x, 
                            bounding_box_center_y, 
                            bounding_box_width, 
                            bounding_box_height,
                            32,
                            24,
                            &corrected_bb_x, 
                            &corrected_bb_y, 
                            &corrected_bb_width, 
                            &corrected_bb_height);
                            
  Serial.println("corrected_bb_x: " + String(corrected_bb_x) + " " +
                 "corrected_bb_y: " + String(corrected_bb_y) + " " +
                 "corrected_bb_width: " + String(corrected_bb_width) + " " +
                 "corrected_bb_height: " + String(corrected_bb_height) + " ");

  // 畫 bounding box
  draw_bounding_box(rgb888_buf_320x240, 320, 240, 3, corrected_bb_x, corrected_bb_y, corrected_bb_width, corrected_bb_height,255,255,255);
  
  fmt2jpg(rgb888_buf_320x240, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf, &jpg_size);

  writeFile(SD, file_dir, jpg_buf, jpg_size);
  
  // release image
  esp_camera_fb_return(fb); 
  fb = NULL;
  free(jpg_buf);
  jpg_buf = NULL;
}

// SD card write file
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
  
  Serial.printf("Writing file: %s\n", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial.println("Failed to open file for writing");
      return;
  }
  if(file.write(data, len) != len){
      Serial.println("Write failed");
  }
  file.close();
}

// 這個是 bounding box 的 x, y 在左上角
void draw_bounding_box(uint8_t * image_buf, 
                        uint16_t image_width, 
                        uint16_t image_height, 
                        uint8_t channels, 
                        uint16_t bounding_box_x, 
                        uint16_t bounding_box_y, 
                        uint16_t bounding_box_width, 
                        uint16_t bounding_box_height,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b)
{
  
  //uint32_t center_index = find_image_buffer_index(image_width, bounding_box_center_x, bounding_box_center_y, channels);
  uint32_t bd_box_leftTop_x = bounding_box_x;
  uint32_t bd_box_leftTop_y = bounding_box_y;
  uint32_t bd_box_leftBottom_x = bounding_box_x;
  uint32_t bd_box_leftBottom_y = bounding_box_y + bounding_box_height;
  uint32_t bd_box_rightTop_x = bounding_box_x + bounding_box_width;
  uint32_t bd_box_rightTop_y = bounding_box_y;
  uint32_t bd_box_rightBottom_x = bounding_box_x + bounding_box_width;
  uint32_t bd_box_rightBottom_y = bounding_box_y + bounding_box_height;


  // draw top line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + i, bd_box_leftTop_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, r, g, b);
  }

  // draw bottom line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftBottom_x + i, bd_box_leftBottom_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, r, g, b);
  }

  // draw left line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, r, g, b);
  }

  // draw right line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + bounding_box_width, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, r, g, b);
  }
}


uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels)
{
  return (image_width * y + x) * channels;
}

void draw_pixel(uint8_t * image_buf, uint32_t index, uint8_t channels, uint8_t r, uint8_t g, uint8_t b){
  image_buf[index] = b;
  image_buf[index + 1] = g;
  image_buf[index + 2] = r;
  index = index + channels;
}

void bounding_box_correction( uint16_t original_buf_width, 
                              uint16_t original_buf_height, 
                              uint16_t original_bb_x, 
                              uint16_t original_bb_y, 
                              uint16_t original_bb_width, 
                              uint16_t original_bb_height,
                              uint16_t corrected_buf_width,
                              uint16_t corrected_buf_height,
                              uint16_t * corrected_bb_x, 
                              uint16_t * corrected_bb_y, 
                              uint16_t * corrected_bb_width, 
                              uint16_t * corrected_bb_height)
{
  *corrected_bb_x = original_bb_x * ((float)corrected_buf_width / (float)original_buf_width);
  *corrected_bb_y = original_bb_y * ((float)corrected_buf_height / (float)original_buf_height);
  *corrected_bb_width = original_bb_width * ((float)corrected_buf_width / (float)original_buf_width);
  *corrected_bb_height = original_bb_height * ((float)corrected_buf_height / (float)original_buf_height);
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



/*
void draw_line(char draw_direction[32],
                uint8_t * image_buf, 
                uint16_t image_width, 
                uint16_t image_height, 
                uint8_t channels, 
                uint16_t bounding_box_center_x, 
                uint16_t bounding_box_center_y, 
                uint16_t bounding_box_width, 
                uint16_t bounding_box_height)
{
        
  uint32_t bd_box_leftTop_x = bounding_box_center_x - (bounding_box_width / 2);
  uint32_t bd_box_leftTop_y = bounding_box_center_y - (bounding_box_height / 2);
  uint32_t bd_box_leftBottom_x = bounding_box_center_x - (bounding_box_width / 2);
  uint32_t bd_box_leftBottom_y = bounding_box_center_y + (bounding_box_height / 2);


  // draw top line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + i, bd_box_leftTop_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw bottom line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftBottom_x + i, bd_box_leftBottom_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw left line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw right line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + bounding_box_width, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }
}

*/
